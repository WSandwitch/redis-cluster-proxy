# Redis Cluster Proxy

> This is a fork of RedisLabs's cluster proxy, targeting compatibility with **Redis 7.2** (the last BSD-licensed Redis 7.x release) so that it can be used as a drop-in replacement for a single Redis instance in RESP2 mode.
>
> Main differences from the original proxy:
> - multiple databases are emulated (`SELECT n` switches the key prefix, see "Database emulation" below); `MOVE` and `COPY ... DB` work across databases.
> - `INFO` returns the info of the first master node plus the proxy's own sections.
> - `HELLO 2` is supported (RESP2); `HELLO 3` returns `NOPROTO`.
> - `CLIENT SETNAME/GETNAME/ID/INFO/SETINFO`, `QUIT`, `RESET`, `READONLY`, `READWRITE` and `ASKING` are handled locally.
> - `CONFIG`, `SCRIPT` and `FUNCTION` are supported (`CONFIG SET`, `SCRIPT LOAD/FLUSH/EXISTS` and `FUNCTION` mutations are broadcast to all masters).
> - Cross-slot queries are enabled by default (they are not atomic); an optional lock mode and a strict `error` mode are available (see "Cross-slot write policy" below).
> - Pub/Sub is not supported yet: Pub/Sub commands return an explicit unsupported-command error.
> - RESP3, client-side caching (`CLIENT TRACKING`) and cluster administration commands are not supported.
> - `HELLO`/`INFO` report a standalone Redis instance and cluster-only commands reply with `This instance has cluster support disabled` (see "Standalone emulation" below).
>
> It also integrates various pull requests with **bug fixes** (see commits list for details).
>
> Lastly, it includes a `Dockerfile` so the image is automatically built on Docker Hub. The image name is [jeko/redis-cluster-proxy](https://hub.docker.com/r/jeko/redis-cluster-proxy).
>
> That's all! Now the original description:

Redis Cluster Proxy is a proxy for [Redis](https://redis.io/) Clusters.
Redis has the ability to run in Cluster mode, where a set of Redis instances will take care of failover and partitioning. This special mode requires the use of special clients understanding the Cluster protocol: by using this Proxy instead the Cluster is abstracted away, and you can talk with a set of instances composing a Redis Cluster like if they were a single instance.
Redis Cluster Proxy is multi-threaded and it currently uses, by default, a multiplexing communication model so that every thread has its own connection to the cluster that is shared to all clients belonging to the thread itself.
Anyway, in some special cases (ie. `MULTI` transactions or blocking commands), the multiplexing gets disabled and the client will have its own cluster connection.
In this way clients just sending trivial commands like GETs and SETs will
not require a private set of connections to the Redis Cluster.

So, these are the main features of Redis Cluster Proxy:

- Routing: every query is automatically routed to the correct node of the cluster
- Multithreaded
- Both multiplexing and private connection models supported
- Query execution and reply order are guaranteed even in multiplexing contexts
- Automatic update of the cluster's configuration after `MOVED` errors: when those kinds of errors occur in replies, the proxy automatically updates its internal representation of the cluster by fetching an updated configuration of it and by remapping all the slots. All queries will be re-executed after the update is completed, so that, from the client's point-of-view, everything flows as normal (the clients won't receive the MOVED error: they will directly receive the expected replies after the cluster configuration has been updated). `ASK` replies are handled without a full reconfiguration: the request is re-sent to the indicated node prefixed with `ASKING`.
- Cross-slot/Cross-node queries: many commands involving multiple keys belonging to different slots (or even to different cluster nodes) are supported. Those commands will split the query into multiple queries that will be routed to different slots/nodes. Reply handling for those commands is command-specific. Some commands, such as `MGET`, will merge all the replies as if they were a single reply. Other commands such as `MSET` or `DEL` will sum the results of all the replies. Since those queries actually break the atomicity of the command, they are enabled by default but can be disabled with `--disable-cross-slot` (see below). **WARNING**: cross-slot queries are not atomic: if a client disconnects or a node fails in the middle of a cross-slot query, only part of it may be executed.
- Some commands with no specific node/slot such as `DBSIZE` are delivered to all the nodes and the replies will be map-reduced in order to give a sum of all the values contained in all the replies.
- The additional `PROXY` command that can be used to perform some proxy-specific actions.

# Build

Redis Cluster Proxy should run without issues on most POSIX systems (Linux, macOS/OSX, NetBSD, FreeBSD) and on the same platforms supported by Redis.

**Anyway**, it requires C11 and its **atomic variables**, so please ensure that your compiler is supporting both C11 and atomic variables (`_Atomic`).
As for **GCC**, those features are supported by version 4.9 or later.

In order to build it, just type:

`% make`

If you need a 32-bit binary, use:

`% make 32bit`

If you need a verbose build, use the `V` option:

`% make V=1`

If you need to rebuild dependencies, use:

`% make distclean`

And, finally, if you want to launch tests, just type:

`% make test`

**Note:** by default, tests use the `redis-server` that is installed on your system (the one that is found in your `$PATH`). If you need to use another `redis-server`, use the environment variable `REDIS_HOME`, ie:

`% REDIS_HOME=/path/to/my/redis/src make test`

As you can see, the make syntax (but also the output style) is the same used in Redis, so it will be familiar to Redis users.

# Install

In order to install Redis Cluster Proxy into /usr/local/bin just use:

`% make install`

You can use make PREFIX=/some/other/directory install if you wish to use a different destination.

# Usage

Redis Cluster Proxy attaches itself to an already running Redis cluster.
The binary will be compiled inside the `src` directory.
The basic usage is:

`./redis-cluster-proxy CLUSTER_ADDRESS`

where `CLUSTER_ADDRESS` is the host address of any cluster's instance (we call it the *entry point*), and it can be expressed in the form of an `IP:PORT` for TCP connections, or as UNIX socket by specifying the file name.

For example:

`./redis-cluster-proxy 127.0.0.1:7000`

`./redis-cluster-proxy /path/to/entry-point.socket`

It is also possible to specify more entry-points as multiple addresses. The proxy will use the first reachable entry-point in order to connect to the cluster and fetch the configuration of the cluster itself.
This can be useful since a single entry-point could be down, so you can use multiple addresses to make the proxy more reliable.

Example:

`./redis-cluster-proxy 127.0.0.1:7000 127.0.0.1:7001 127.0.0.1:7002`

If you need a basic help, just run it with the canonical `-h` or `--help` option.

`./redis-cluster-proxy -h`

By default, Redis Cluster Port will listen on port 7777, but you can change it with the `-p` or `--port` option.
Furthermore, by default, Redis Cluster Port will bind all available network interfaces to listen to incoming connections.
You can bind to specific interfaces by using the `--bind` options. You can bind  a single interface or you can bind multiple interfaces by using the `--bind` option more the once.

You can also tell Redis Cluster Port to listen upon a UNIX socket, by using the `--unixsocket` option to specify the socket filename and, optionally, the `--unixsocketperm` to set socket file permissions.

If you want to only listen on the UNIX socket, set `--port` to 0, so that the proxy won't listen on TCP sockets at all.

Examples:

Listen on port 7888

`./redis-cluster-proxy --port 7888 127.0.0.1:7000`

Listen on default port and bind only 127.0.0.1:

`./redis-cluster-proxy --bind 127.0.0.1 127.0.0.1:7000`

Listen on port 7888 and bind multiple interfaces:

`./redis-cluster-proxy --port 7888 --bind 192.168.0.10 --bind 10.0.0.10 127.0.0.1:7000`

Listen on UNIX socket and disable TCP connections

`./redis-cluster-proxy --unixsocket /path/to/proxy.socket --port 0 127.0.0.1:7000`

You can change the number of threads using the `--threads` option.

You can also use a configuration file instead of passing arguments by using the `-c` options, ie:

`redis-cluster-proxy -c /path/to/my/proxy.conf 127.0.0.1:7000`

You can find an example `proxy.conf` file inside the main Redis Cluster Proxy's directory.

After launching it, you can connect to the proxy as if it were a normal Redis server (however make sure to understand the current limitations).

You can then connect to Redis Cluster Proxy using the client you prefer, ie:

`redis-cli -p 7777`

# Private connections pool

Every thread has its own connections pool that contains *ready-to-use* private connections to the cluster, whose sockets are pre-connected in the same moment they are created.
This allows clients requiring private connections (ie. after commands such as `MULTI` or blocking commands) to immediately use a connection that is probably already connected to the cluster, instead of reconnecting to the cluster from scratch (a situation that could slow-down the sequence execution of the queries from the point-of-view of the client itself).
Every connection pool has a predefined size, and it's not allowed to create more connections than those allowed by its size.
The size of the connection pool can be configured via the `--connections-pool-size` option (by default it's 10).
When the pool runs out of connections, every new client requiring a private connection will create a new private connection from scratch and it will have to connect to the cluster and wait for the connection to be established. In this case, the connection model will be "lazy", meaning that the sockets of the new connection will connect to a particular node of the cluster only when the query will require a connection to that node.
Every thread will re-populate its own pool after the number of connections will drop below the specified minimum, that by default is the same of the size of the pool itself, and that can be configured via the `--connections-pool-min-size` option. The population rate and interval can be defined by the `--connections-pool-spawn-every` (interval in milliseconds) and `--connections-pool-spawn-rate` (number of new connection at every interval).

So:

```
redis-cluster-proxy --connections-pool-size 20 connections-pool-min-size 15 --connections-pool-spawn-rate 2 --connections-pool-spawn-every 500 127.0.0.1:7000
```

Means: *"create a connection pool containing 20 connections (maximum), and re-populate it when the number of connections drops below 15, by creating 2 new connections every 500 milliseconds"*.

Remember that every pool will be completely populated when the proxy starts.
It's also important to remark that when clients owning a private connection will disconnect, their thread will try to recycle their private connection in order to add it again to the pool if the pool itself is not already full.

# Password-protected clusters and Redis ACL

If your cluster nodes are protected with a password, you can use the `-a`, `--auth` command-line options or the `auth` option in a configuration file in order to specify an authentication password.
Furthermore, if your cluster is using the new [ACL](https://redis.io/topics/acl) implemented in Redis 6.0 and it has multiple users, you can even authenticate with a specific user by using the `--auth-user` command-line option (or `auth-user` in a config file) followed by the username.
Examples:

`redis-cluster-proxy -a MYPASSWORD 127.0.0.1:7000`

`redis-cluster-proxy --auth MYPASSWORD 127.0.0.1:7000`

`redis-cluster-proxy --auth-user MYUSER --auth MYPASSWORD 127.0.0.1:7000`

The proxy will use these credentials to authenticate to the cluster and fetch the cluster's internal configuration, but it will also automatically authenticate all clients with the provided credentials.
So, **all clients** that will connect to the proxy will be **automatically authenticated** with the user that is specified by `--auth-user` or with the `default` user if no user has been specified, **without the need** to call the `AUTH` command by themselves.
Anyway, if any client wants to be authenticated with a different user, it always can call the Redis `AUTH` command (documented [here](https://redis.io/commands/auth)): in this case, the client will use a private connection instead of the shared, multiplexed connection, and it will authenticate with another user.

# Cross-slots queries

Cross-slots queries use multiple keys belonging to different slots or even different nodes.
Since their execution is not guaranteed to be atomic (so, they can actually break the atomic design of many Redis commands), they are **enabled by default** to make the proxy behave like a regular Redis instance. You can disable them with the `--disable-cross-slot` option, or by setting `disable-cross-slot yes` in your config file. You can also change this behavior while the proxy is running by using the special `PROXY` command (see below).

**WARNING**: cross-slot queries are not atomic. If the client disconnects or a node fails while the query is being executed, only part of the command may be applied.

**Note**: cross-slots queries are not supported by all the commands, even if the feature is enabled (ie. you cannot use it with `EVAL` or `ZUNIONSTORE` and many other commands). In that case, you'll receive a specific error reply. You can fetch a list of commands that cannot be used in cross-slots queries by using the `PROXY` command (see below).

# Cross-slot write policy

When cross-slot queries are enabled, the proxy can handle multi-key commands whose keys belong to different slots by splitting them into multiple queries. Such queries are not atomic, so three policies are available (`--cross-slot-write-policy`):

- `allow` (default): execute cross-slot write queries. Fast, but a crash or a network failure in the middle of the query can leave the data partially updated.
- `error`: reject cross-slot write queries with a `CROSSSLOT` error, like a real Redis Cluster does. Use hash tags (eg. `{user}:1`, `{user}:2`) for operations that must be atomic.
- `lock`: serialize cross-slot write queries using a global lock stored in the cluster, so that two cross-slot write queries issued through **any** proxy using the same protocol cannot interleave.

The lock is stored under the `{__proxy:lock}` key (the hash tag makes every proxy use the same slot). Its TTL (default: 10000 ms) and the maximum time a client can wait for it (default: 5000 ms) can be configured with `--cross-slot-lock-ttl` and `--cross-slot-lock-timeout`.

**Limitations of the `lock` policy**:
- it does **not** block single-key operations: a plain `SET` issued while a cross-slot query holds the lock can still be interleaved with it (use `error` plus hash tags if you need strict atomicity);
- it does **not** provide crash-atomicity: if the proxy dies in the middle of a cross-slot query, part of it may remain applied (the lock is eventually released by its TTL);
- every proxy that takes part in the serialization must use the same lock configuration.

# Database emulation

A Redis Cluster only has DB 0, while clients of a standalone Redis can use `SELECT` to choose one of many databases. To behave like a drop-in replacement for a standalone Redis instance, the proxy emulates multiple databases by prefixing every key with the database number.

The physical key of a logical key `foo` in database `n` is:

```
<n>/<hash-tag>foo
```

where `<hash-tag>` is a three-character tag chosen so that:

```
CRC16(hash-tag) % 16384 == slot(foo)
```

Keeping the original slot means that the same key in two different databases lives on the **same cluster node**, so `MOVE` and `COPY` between databases are single-slot operations that Redis can execute atomically. It also means that enabling database emulation does not change the slot layout of the keys.

Configuration:

```
databases 16
db-prefix "%d/"
db-scan-count 1000
```

The same options are available on the command line as `--databases`, `--db-prefix` and `--db-scan-count`.

- `SELECT n` accepts `0 <= n < databases`, replies with `OK` and switches the prefix; other values reply with `DB index is out of range`.
- `SELECT` is not allowed inside `MULTI` (it could split the transaction across different slots).
- `RESET` goes back to database 0, `CLIENT INFO` reports the selected database.
- The prefix is applied to **all** databases, including DB 0 (`0/foo`), so that no key is visible from two databases.
- `KEYS`, `SCAN` (including its `MATCH` option), `RANDOMKEY`, `DBSIZE`, `FLUSHDB`, `MOVE`, `COPY ... DB` and the blocking/pop/stream commands are database-aware.
- `DBSIZE`, `FLUSHDB` and `SWAPDB` are implemented with `SCAN` and are therefore O(N); their `COUNT` can be tuned with `--db-scan-count`.
- `SCAN` cursors encode the database they belong to: reusing a cursor in another database replies with `invalid cursor`.
- `SWAPDB` swaps the two databases one key at a time (`SCAN` + `RENAME`) and is therefore **not atomic**: clients must not access the two databases while it is running, and a failure in the middle leaves them partially swapped. To avoid collisions, the keys are moved through a temporary database outside the range that clients can `SELECT`; keys left there by a crash are removed the next time the proxy starts (provided `databases` and `db-prefix` are unchanged).
- `SWAPDB` does not touch keys watched with `WATCH`: since it only renames the keys that actually exist, a transaction watching a key that exists in neither of the two databases is not invalidated (the real command invalidates it).
- Concurrent `SWAPDB` commands are not supported: like any other access to the two databases during a swap, they must be serialized by the client.
- `INFO keyspace` is omitted, since all the emulated databases share the cluster's DB 0; use `DBSIZE` instead.
- `COPY src dst DB n` is allowed only if the two keys hash to the same slot (which is always the case when `src == dst`), otherwise it replies with `CROSSSLOT`.

Use `disable-db-emulation yes` (or `--disable-db-emulation`) to turn the feature off: in that case the proxy behaves like a Redis Cluster and only DB 0 is available.

**Limitations**:
- scripts executed with `EVAL`/`FCALL` see the **physical** key names (`KEYS[1]` is `n/<tag>foo`), so they must only use `KEYS[i]` and never build key names from `ARGV`;
- `FUNCTION` libraries and `SCRIPT` caches are global per node: they are not isolated per database;
- `SORT BY`/`GET` patterns are rewritten for the current database, but Redis Cluster itself denies `SORT BY`;
- enabling database emulation on a cluster that already contains unprefixed keys makes those keys invisible (the slots are preserved, so they can be migrated by renaming them).

# Standalone emulation

By default (`--emulate-standalone`) the proxy replies like a standalone Redis instance:

- `HELLO` reports `mode:standalone` and `INFO` reports `redis_mode:standalone`, `cluster_enabled:0`;
- `CLUSTER`, `READONLY`, `READWRITE` and `ASKING` reply with `ERR This instance has cluster support disabled`, exactly like a standalone Redis.

Use `--disable-standalone-emulation` to reply like a Redis Cluster instead.

# The PROXY command

The `PROXY` command will allow you to get specific info or perform actions that are specific to the proxy. The command has various subcommands, here's a little list:

- PROXY CONFIG GET|SET option [value]

  It can be used to get or set a specific option of the proxy, where the options
  are the same used in the command line arguments (without the `--` prefix) or specified in the config file.
  Not all the options can be changed (some of them, ie. `threads`, are read-only).
  
  Examples:

  ```
  PROXY CONFIG GET threads
  PROXY CONFIG SET log-level debug
  PROXY CONFIG SET enable-cross-slot 1
  ```
- PROXY MULTIPLEXING STATUS|OFF

  Get the status of multiplexing connection model for the calling client,
  or disable multiplexing by activating a private connection for the calling client.
  Examples:

  ```
  -> PROXY MULTIPLEXING STATUS
  -> Reply: "on"
  -> PROXY MULTIPLEXING off
  ```

- PROXY INFO

  Returns info specific to the cluster, similarly to the `INFO` command in Redis.

- PROXY COMMAND [UNSUPPORTED|CROSSSLOTS-UNSUPPORTED]

  Returns a list of all the Redis commands handled (known) 
  by Redis Cluster Proxy, in a similar fashion to Redis `COMMAND` function.
  The returned reply is a nested Array: every command will be an item of the 
  top-level array and it will be an array itself, containing the following 
  items: command name, arity, first key, last key, key step, supported.
  The last item ("supported") indicates whether the command is currently 
  supported by the proxy.

  The optional third argument can be used as a filter, with the following options:
  - `UNSUPPORTED`: only lists unsupported commands
  - `CROSSSLOTS-UNSUPPORTED`: only lists commands that cannot be used with 
  cross-slots queries, even if cross-slots queries have been enabled in the proxy's configuration.

- PROXY CLIENT <subcmd>

  Perform client-specific actions, ie:

    - `PROXY CLIENT ID`: get the current client's internal ID

    - `PROXY CLIENT THREAD`: get the current client's thread

- PROXY CLUSTER [subcmd]

  Perform actions related to the cluster associated with the calling client, ie:

    - `PROXY CLUSTER` or `PROXY CLUSTER INFO` Get info about the cluster. Info is an array whose elements are in the form of name/value pairs, where the names are specific features such as `status`, `connection`, and so on. You can also retrieve info for a single specific feature, ie. by calling `PROXY CLUSTER STATUS`.
    Below there's a list of common info that can be retrieved:

        - `status`: Current status of the cluster, can be `updating`, `updated` or `broken`
        - `connection`: Connection type, that can be `shared` if the client is working inside a multiplexing context (so the connection is shared with all the clients of the thread), or `private` if the client is using its own private connection.
        - `nodes`: A nested array containing the list of all the master nodes
                   of the cluster. Every node is another nested array, containing name/value pairs.

    - `PROXY CLUSTER UPDATE`: request an update of the current cluster's configuration.


  Examples:

  ```
  -> PROXY CLUSTER

  1) status
  2) updated
  3) connection
  4) shared
  5) nodes
  6) 1)  1) name
         2) 8d829c8b66f67dd9c4adad16e5c0a4c82aadd810
         3) ip
         4) 127.0.0.1
         5) port
         6) (integer) 7002
         7) slots
         8) (integer) 5462
         9) replicas
        10) (integer) 1
        11) connected
        12) (integer) 1
    ...

  ```

- PROXY LOG [level] MESSAGE

    Log `MESSAGE` to Proxy's log, for debugging purposes.

    The optional `level` can be used to define the log level:

    `debug`, `info`, `success`, `warning`, `error` (default is `debug`)
    
- PROXY DEBUG <subcommand>

    Perform different actions for debugging purpose, where `subcommand` can be:

    - `SEGFAULT`: crash the proxy with sigsegv

    - `ASSERT`: crash the proxy with an assertion failure

- PROXY SHUTDOWN [ASAP]

    Shutdown the proxy. The optional `ASAP` option makes the proxy exit immeditely (dirty exit).

- PROXY HELP

    Get help for the PROXY command

# Commands that act differently from standard Redis commands or that have special behavior

- PING: `PONG` is replied directly by the proxy
- MULTI: disables multiplexing for the calling client by creating a private
         connection in the client itself. **Note**: since it's required to be
         atomic, cross-slots queries cannot work inside a multi transaction.
- DBSIZE: sends the query to all nodes in the cluster and sums their replies,
          so that the result will be the total number of keys in the whole
          cluster.
- SWAPDB: when database emulation is enabled it is implemented with `SCAN` +
          `RENAME`, so it is slow and not atomic (see "Database emulation"
          below).
- SCAN: performs the scan on all the master nodes of the cluster. The cursor returned to the client encodes both the real Redis cursor and the index of the node that has to be scanned next, as `(real_cursor + 1) * 10000 + node_index`. Cursors are plain decimal numbers, so clients can safely convert them to integers. The final cursor (when the scan is complete) is `0`.
- INFO: collected from all the master nodes. Counter fields that span the whole cluster (currently `blocked_clients` and `rdb_changes_since_last_save`) are summed, so that the proxy behaves like a single instance.
- MEMORY: `MEMORY USAGE <key>` is routed to the node owning the key (the key is prefixed when database emulation is enabled); the other subcommands are executed on the first master.
- CLIENT UNBLOCK: returns an explicit error. Blocking commands are executed on private connections, whose node-side client IDs are not the IDs returned by `CLIENT ID`, so unblocking by ID would target the wrong connection.
- MULTI/EXEC: in database emulation mode the replies of the queued commands are returned by the node as part of a single `EXEC` array, so the proxy cannot strip the database prefix from key names contained in them (e.g. the key returned by a blocking pop executed inside a transaction).


For a list of all known commands (both supported and unsupported) and their 
features, see [COMMANDS.md](COMMANDS.md)

# Compatibility testing

Besides the Ruby integration tests in `test/`, the proxy can be checked against
the upstream Redis test suite (the proxy targets Redis 7.2). The runner starts a
throw-away cluster and a proxy and executes selected units of the suite in
"external server" mode:

```sh
test/upstream/run.sh                       # default subset
test/upstream/run.sh unit/keyspace         # a single unit
```

The suite is not vendored: see [test/upstream/README.md](test/upstream/README.md)
for how to fetch it and for the list of tests that cannot pass because they
need features the proxy does not provide by design (RESP3, DEBUG, replication
streams, persistence, ...).

# Current status

This project is currently alpha code that is indented to be evaluated by the community in order to get suggestions and contributions. We discourage its usage in any production environment.
