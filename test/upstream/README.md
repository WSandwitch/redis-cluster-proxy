# Upstream Redis test suite

The proxy is meant to look like a standalone Redis 7.2 instance, so the best
compatibility check is the Redis test suite itself.

This directory contains a small runner that starts a throw-away cluster and a
proxy, then executes selected units of the upstream suite in
"external server" mode (`runtest --host ... --port ...`), i.e. without letting
the suite start its own server.

## Requirements

- a built proxy (`make`);
- Redis binaries (`redis-server`, `redis-cli`) of the version to test
  against (default: `/tmp/redis72`, override with `REDIS_HOME`);
- Tcl 8.5+ (`tclsh`);
- the Redis test suite sources (not vendored here).

Fetch the suite of the matching Redis version (the proxy targets 7.2):

```sh
curl -sSL -o /tmp/redis-7.2.tar.gz \
    https://github.com/redis/redis/archive/refs/heads/7.2.tar.gz
mkdir -p /tmp/redis-tests-src
tar xzf /tmp/redis-7.2.tar.gz -C /tmp/redis-tests-src
# => /tmp/redis-tests-src/redis-7.2
```

## Run

```sh
test/upstream/run.sh                       # default subset
test/upstream/run.sh unit/protocol         # one unit
KEEP=1 test/upstream/run.sh unit/keyspace  # keep logs and the temp cluster
```

Useful environment variables:

| Variable            | Default                             |
|---------------------|-------------------------------------|
| `REDIS_HOME`        | `/tmp/redis72`                      |
| `REDIS_TESTS_DIR`   | `/tmp/redis-tests-src/redis-7.2`    |
| `PROXY_BIN`         | `src/redis-cluster-proxy`           |
| `PROXY_PORT`        | `7888`                              |
| `CLUSTER_BASE_PORT` | `7000`                              |
| `CLIENTS`           | `1`                                 |
| `TIMEOUT`           | `1800` seconds                      |

The runner always filters out tests that need features the proxy does not
provide by design, using the `needs:*` tags of the upstream suite:

```sh
--tags -needs:debug -needs:repl -needs:pfdebug -needs:save \
       -needs:latency -needs:local-process -needs:config-maxmemory \
       -needs:config-resetstat -resp3
```

It also passes `--ignore-encoding`, since object encodings are an
implementation detail of the nodes and are not affected by the proxy.

## Notes

`unit/multi` is not part of the default subset: the upstream file calls
`DEBUG SET-ACTIVE-EXPIRE` at the top level (outside any test), which the proxy
does not expose. MULTI/EXEC and WATCH are covered by the proxy's own Ruby
tests (`test/tests/multi.rb`); the only known upstream incompatibility is that
keys inside the `EXEC` reply array are not rewritten (see `skip.txt`).

## Skip list

`skip.txt` lists tests that cannot pass because they depend on features the
proxy does not provide by design:

- RESP3 (`HELLO 3` replies `NOPROTO`);
- replication streams (`SYNC`/`PSYNC`) used to check command propagation;
- `DEBUG`, persistence, replication and cluster administration commands;
- tests that build key names from `ARGV` inside scripts (the proxy's database
  emulation exposes physical key names to scripts: use `KEYS[i]`).

Keep the list as short as possible: every entry is a documented
incompatibility, and entries that start passing again should be removed.

## Database emulation

By default the proxy emulates 16 databases, so the suite's default database
(`SELECT 9`) works normally. The physical keys are prefixed (`9/{tag}key`), but
the tests only see logical names thanks to the reply rewriting.

To exercise the "cluster mode" behaviour (no database emulation), start the
proxy with `--disable-db-emulation --disable-standalone-emulation`; note that
`SELECT`-based tests will then fail.
