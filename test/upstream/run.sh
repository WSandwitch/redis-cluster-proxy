#!/usr/bin/env bash
#
# Run the upstream Redis test suite against redis-cluster-proxy.
#
# The suite is not vendored in this repository: fetch the tests of the Redis
# version you want to test against (see README.md) and point REDIS_TESTS_DIR to
# the extracted source tree. The script starts a throw-away cluster and a
# proxy, runs the requested units in external-server mode and cleans
# everything up.
#
# Usage:
#   test/upstream/run.sh [unit ...]
#
# Examples:
#   test/upstream/run.sh                     # default subset
#   test/upstream/run.sh unit/protocol unit/keyspace
#   KEEP=1 test/upstream/run.sh unit/scan    # keep logs/temp dir
#
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)

REDIS_HOME=${REDIS_HOME:-/tmp/redis72}
REDIS_TESTS_DIR=${REDIS_TESTS_DIR:-/tmp/redis-tests-src/redis-7.2}
PROXY_BIN=${PROXY_BIN:-$ROOT/src/redis-cluster-proxy}
PROXY_PORT=${PROXY_PORT:-7888}
CLUSTER_BASE_PORT=${CLUSTER_BASE_PORT:-7000}
CLUSTER_NODES=${CLUSTER_NODES:-3}
CLIENTS=${CLIENTS:-1}
LOG_LEVEL=${LOG_LEVEL:-info}
SKIP_FILE=${SKIP_FILE:-$ROOT/test/upstream/skip.txt}
KEEP=${KEEP:-0}
TIMEOUT=${TIMEOUT:-1800}
WORKDIR=${WORKDIR:-}

if [ "$#" -gt 0 ]; then
    UNITS=("$@")
else
    UNITS=(
        unit/protocol
        unit/keyspace
        unit/scan
        unit/type/string
        unit/type/incr
        unit/type/hash
        unit/type/set
        unit/type/list
        unit/type/zset
        unit/expire
        unit/bitops
        unit/geo
        unit/hyperloglog
        unit/dump
        unit/quit
    )
fi

die() { echo "ERROR: $*" >&2; exit 1; }

[ -x "$REDIS_HOME/redis-server" ] || die "redis-server not found in $REDIS_HOME (set REDIS_HOME)"
[ -x "$REDIS_HOME/redis-cli" ] || die "redis-cli not found in $REDIS_HOME (set REDIS_HOME)"
[ -x "$REDIS_TESTS_DIR/runtest" ] || die "Redis test suite not found in $REDIS_TESTS_DIR (set REDIS_TESTS_DIR)"
[ -x "$PROXY_BIN" ] || die "proxy binary not found in $PROXY_BIN (run make first)"

if [ -z "$WORKDIR" ]; then
    WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/rcp-upstream.XXXXXX")
else
    mkdir -p "$WORKDIR"
fi
mkdir -p "$WORKDIR/logs"

PROXY_PID=""
NODE_PIDS=()

cleanup() {
    set +e
    if [ -n "$PROXY_PID" ] && kill -0 "$PROXY_PID" 2>/dev/null; then
        kill "$PROXY_PID" 2>/dev/null
        wait "$PROXY_PID" 2>/dev/null
    fi
    for pid in "${NODE_PIDS[@]:-}"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    sleep 0.2
    if [ "$KEEP" = "1" ]; then
        echo "Keeping work directory: $WORKDIR"
    else
        rm -rf "$WORKDIR"
    fi
}
trap cleanup EXIT INT TERM

echo "Work directory: $WORKDIR"
echo "Redis:          $REDIS_HOME ($("$REDIS_HOME/redis-server" --version | head -1))"
echo "Tests:          $REDIS_TESTS_DIR"
echo "Proxy:          $PROXY_BIN"
echo

# ---------------------------------------------------------------- cluster ----
for ((i = 0; i < CLUSTER_NODES; i++)); do
    port=$((CLUSTER_BASE_PORT + i))
    dir="$WORKDIR/$port"
    mkdir -p "$dir"
    "$REDIS_HOME/redis-server" \
        --port "$port" --dir "$dir" \
        --cluster-enabled yes --cluster-config-file nodes.conf \
        --cluster-node-timeout 5000 --appendonly no --save "" \
        --daemonize no --protected-mode no --bind 127.0.0.1 \
        --logfile "$dir/log" --pidfile "$dir/pid" \
        >/dev/null 2>&1 &
    NODE_PIDS+=($!)
done

wait_for_ping() {
    local port=$1 tries=${2:-100} i
    for ((i = 0; i < tries; i++)); do
        if "$REDIS_HOME/redis-cli" -p "$port" ping 2>/dev/null | grep -q PONG; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

for ((i = 0; i < CLUSTER_NODES; i++)); do
    wait_for_ping $((CLUSTER_BASE_PORT + i)) || die "cluster node $((CLUSTER_BASE_PORT + i)) did not start"
done

addrs=()
for ((i = 0; i < CLUSTER_NODES; i++)); do
    addrs+=("127.0.0.1:$((CLUSTER_BASE_PORT + i))")
done
"$REDIS_HOME/redis-cli" --cluster create "${addrs[@]}" \
    --cluster-replicas 0 --cluster-yes >"$WORKDIR/logs/cluster-create.log" 2>&1 \
    || die "failed to create the cluster (see $WORKDIR/logs/cluster-create.log)"

# ------------------------------------------------------------------ proxy ----
"$PROXY_BIN" --port "$PROXY_PORT" --threads 2 --log-level "$LOG_LEVEL" \
    "127.0.0.1:$CLUSTER_BASE_PORT" >"$WORKDIR/logs/proxy.log" 2>&1 &
PROXY_PID=$!

# PING is replied locally, so also verify that a key can actually be routed
# to the cluster (the proxy fetches the cluster configuration at startup).
wait_for_proxy() {
    local port=$1 tries=${2:-200} i
    for ((i = 0; i < tries; i++)); do
        if "$REDIS_HOME/redis-cli" -p "$port" SET __proxy_ready__ 1 2>/dev/null |
            grep -q '^OK$'; then
            "$REDIS_HOME/redis-cli" -p "$port" DEL __proxy_ready__ >/dev/null 2>&1
            return 0
        fi
        sleep 0.1
    done
    return 1
}

wait_for_proxy "$PROXY_PORT" 200 || {
    tail -20 "$WORKDIR/logs/proxy.log" >&2
    die "proxy did not start"
}

# ------------------------------------------------------------------- tests ---
skip_args=()
if [ -f "$SKIP_FILE" ]; then
    while IFS= read -r line; do
        line="${line%%#*}"
        line="$(echo "$line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
        [ -n "$line" ] && skip_args+=(--skiptest "$line")
    done < "$SKIP_FILE"
fi

single_args=()
for unit in "${UNITS[@]}"; do
    single_args+=(--single "$unit")
done

echo "Running: ${UNITS[*]}"
echo

set +e
( cd "$REDIS_TESTS_DIR" && timeout "$TIMEOUT" ./runtest \
    --host 127.0.0.1 --port "$PROXY_PORT" --clients "$CLIENTS" \
    --tags -needs:debug --tags -resp3 --tags -needs:repl \
    --tags -needs:pfdebug --tags -needs:save --tags -needs:latency \
    --tags -needs:local-process --tags -needs:config-maxmemory \
    --tags -needs:config-resetstat --tags -cluster:skip --ignore-encoding \
    "${skip_args[@]}" "${single_args[@]}" ) \
    | tee "$WORKDIR/logs/runtest.log"
status=${PIPESTATUS[0]}
set -e

echo
if [ "$status" -eq 0 ]; then
    echo "Upstream tests passed."
else
    echo "Upstream tests failed (exit $status). Log: $WORKDIR/logs/runtest.log"
fi
exit "$status"
