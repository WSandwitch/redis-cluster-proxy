require 'redis'
require 'hiredis'

setup &RedisProxyTestCase::GenericSetup

$numclients = $options[:clients] || 2

# Return the list of master nodes.
def db_test_masters
    $main_cluster.masters
end

# Find the physical key corresponding to `logical_key` in database `db` by
# scanning the cluster nodes directly.
def db_test_find_physical(db, logical_key)
    db_test_masters.each{|node|
        begin
            r = Redis.new port: node[:port]
            r.scan_each(match: "*}#{logical_key}"){|key|
                return key if key.start_with?("#{db}/{")
            }
        rescue Redis::CommandError
            next
        end
    }
    nil
end

# Return the cluster slot of `key`, computed by a cluster node.
def db_test_slot(key)
    db_test_masters.each{|node|
        begin
            r = Redis.new port: node[:port]
            return r.call(["CLUSTER", "KEYSLOT", key])
        rescue Redis::CommandError
            next
        end
    }
    nil
end

test "SELECT switches database and keys are isolated" do
    spawn_clients(1){|client, idx|
        assert_equal(client.select(1), "OK")
        assert_equal(client.set("db:alpha", "db1"), "OK")
        assert_equal(client.get("db:alpha"), "db1")
        assert_equal(client.select(0), "OK")
        assert_nil(client.get("db:alpha"))
        assert_equal(client.set("db:alpha", "db0"), "OK")
        assert_equal(client.get("db:alpha"), "db0")
        assert_equal(client.select(1), "OK")
        assert_equal(client.get("db:alpha"), "db1")
        begin
            client.select(16)
            assert(false, "SELECT 16 should fail")
        rescue Redis::CommandError => e
            assert_match(e.message, /DB index is out of range/)
        end
        begin
            client.select("abc")
            assert(false, "SELECT abc should fail")
        rescue Redis::CommandError => e
            assert_match(e.message, /not an integer/)
        end
        assert_equal(client.select(0), "OK")
    }
end

test "KEYS, SCAN and DBSIZE are database-scoped" do
    spawn_clients(1){|client, idx|
        client.select(0)
        client.flushall
        client.set("only:zero", "0")
        client.select(1)
        client.set("only:one", "1")
        client.set("only:one:2", "2")
        keys = client.keys("*")
        assert_class(keys, Array)
        assert_equal(keys.sort, %w(only:one only:one:2))
        assert_equal(client.dbsize, 2)
        scanned = []
        cursor = "0"
        while true
            cursor, batch = client.scan(cursor, count: 100)
            scanned |= batch
            break if cursor.to_i == 0
        end
        assert_equal(scanned.sort, %w(only:one only:one:2))
        client.select(0)
        assert_equal(client.keys("*"), %w(only:zero))
        assert_equal(client.dbsize, 1)
    }
end

test "FLUSHDB only clears the selected database" do
    spawn_clients(1){|client, idx|
        client.select(0)
        client.flushall
        client.set("flush:zero", "0")
        client.select(1)
        client.set("flush:one", "1")
        assert_equal(client.flushdb, "OK")
        assert_equal(client.dbsize, 0)
        assert_equal(client.keys("*"), [])
        client.select(0)
        assert_equal(client.keys("*"), %w(flush:zero))
        client.select(1)
        client.set("flush:one", "1")
        assert_equal(client.flushdb(async: true), "OK")
        assert_equal(client.keys("*"), [])
        client.select(0)
    }
end

test "SCAN cursor cannot be reused in another database" do
    spawn_clients(1){|client, idx|
        client.select(1)
        100.times{|i| client.set("cursor:#{i}", i.to_s)}
        cursor, _ = client.scan("0", count: 1)
        assert(cursor.to_i != 0, "Expected a non-zero cursor")
        client.select(0)
        begin
            client.scan(cursor, count: 1)
            assert(false, "Expected the cursor to be rejected")
        rescue Redis::CommandError => e
            assert_match(e.message, /invalid cursor/)
        end
        client.select(1)
        100.times{|i| client.del("cursor:#{i}")}
        client.select(0)
    }
end

test "MOVE moves a key between databases atomically" do
    spawn_clients(1){|client, idx|
        client.select(0)
        client.flushall
        client.set("move:key", "value")
        assert_equal(client.move("move:key", 1), true)
        assert_nil(client.get("move:key"))
        client.select(1)
        assert_equal(client.get("move:key"), "value")
        client.select(0)
        client.set("move:key", "value0")
        assert_equal(client.move("move:key", 1), false)
        assert_equal(client.get("move:key"), "value0")
        client.select(1)
        assert_equal(client.get("move:key"), "value")
        client.select(0)
        client.del("move:key")
    }
end

test "COPY supports the DB option for same-slot keys" do
    spawn_clients(1){|client, idx|
        client.select(0)
        client.flushall
        client.set("copy:key", "to-copy")
        reply = client.call(["COPY", "copy:key", "copy:key", "DB", 1])
        assert_equal(reply.to_i, 1)
        client.select(1)
        assert_equal(client.get("copy:key"), "to-copy")
        client.select(0)
        begin
            client.call(["COPY", "copy:key", "copy:other", "DB", 1])
            assert(false, "Expected a CROSSSLOT error")
        rescue Redis::CommandError => e
            assert_match(e.message, /CROSSSLOT/)
        end
        client.select(1)
        client.del("copy:key")
        client.select(0)
        client.del("copy:key")
    }
end

test "SWAPDB exchanges the keys of two databases" do
    spawn_clients(1){|client, idx|
        keys = ["swap:same", "swap:only1", "swap:only2"]
        [1, 2].each{|db|
            client.select(db)
            keys.each{|key| client.del(key)}
        }
        client.select(1)
        assert_equal(client.set("swap:same", "one"), "OK")
        assert_equal(client.set("swap:only1", "one-only"), "OK")
        client.select(2)
        assert_equal(client.set("swap:same", "two"), "OK")
        assert_equal(client.set("swap:only2", "two-only"), "OK")
        assert_equal(client.call(["SWAPDB", 1, 2]), "OK")
        assert_equal(client.get("swap:same"), "one")
        assert_equal(client.get("swap:only1"), "one-only")
        assert_nil(client.get("swap:only2"))
        client.select(1)
        assert_equal(client.get("swap:same"), "two")
        assert_equal(client.get("swap:only2"), "two-only")
        assert_nil(client.get("swap:only1"))
        [1, 2].each{|db|
            client.select(db)
            keys.each{|key| client.del(key)}
        }
        client.select(0)
    }
end

test "SWAPDB with an empty database and with DB 0" do
    spawn_clients(1){|client, idx|
        client.select(0)
        client.del("swap0:key")
        client.select(3)
        client.del("swap0:key")
        assert_equal(client.set("swap0:key", "db3"), "OK")
        assert_equal(client.call(["SWAPDB", 0, 3]), "OK")
        assert_nil(client.get("swap0:key"))
        client.select(0)
        assert_equal(client.get("swap0:key"), "db3")
        # Swap back: DB 0 has the key, DB 3 is empty.
        assert_equal(client.call(["SWAPDB", 0, 3]), "OK")
        assert_nil(client.get("swap0:key"))
        client.select(3)
        assert_equal(client.get("swap0:key"), "db3")
        client.del("swap0:key")
        client.select(0)
    }
end

test "SWAPDB preserves TTLs" do
    spawn_clients(1){|client, idx|
        client.select(4)
        client.del("swap:ttl")
        client.select(5)
        client.del("swap:ttl")
        client.select(4)
        assert_equal(client.set("swap:ttl", "value"), "OK")
        assert_equal(client.expire("swap:ttl", 1000), true)
        ttl_before = client.ttl("swap:ttl")
        assert(ttl_before > 0, "Unexpected TTL before the swap: #{ttl_before}")
        assert_equal(client.call(["SWAPDB", 4, 5]), "OK")
        assert_nil(client.get("swap:ttl"))
        client.select(5)
        assert_equal(client.get("swap:ttl"), "value")
        ttl_after = client.ttl("swap:ttl")
        assert(ttl_after > 0 && ttl_after <= ttl_before,
               "Unexpected TTL after the swap: #{ttl_after}")
        client.del("swap:ttl")
        client.select(0)
    }
end

test "SWAPDB edge cases" do
    spawn_clients(1){|client, idx|
        # Swapping a database with itself is a no-op.
        assert_equal(client.call(["SWAPDB", 1, 1]), "OK")
        begin
            client.call(["SWAPDB", 0, 99])
            assert(false, "SWAPDB with an out of range DB should fail")
        rescue Redis::CommandError => e
            assert_match(e.message, /DB index is out of range/)
        end
        begin
            client.call(["SWAPDB", "abc", 1])
            assert(false, "SWAPDB with a non-numeric DB should fail")
        rescue Redis::CommandError => e
            assert_match(e.message, /not an integer/)
        end
        assert_equal(client.multi, "OK")
        begin
            client.call(["SWAPDB", 0, 1])
            assert(false, "SWAPDB inside MULTI should fail")
        rescue Redis::CommandError => e
            assert_match(e.message, /not supported inside MULTI/)
        end
        client.discard
    }
end

test "RESET goes back to database 0" do
    spawn_clients(1){|client, idx|
        client.select(1)
        client.set("reset:key", "1")
        client.call(["RESET"])
        assert_nil(client.get("reset:key"))
        assert_equal(client.set("reset:key", "0"), "OK")
        assert_equal(client.get("reset:key"), "0")
        client.del("reset:key")
        client.select(1)
        client.del("reset:key")
        client.select(0)
    }
end

test "CLIENT INFO reports the selected database" do
    spawn_clients(1){|client, idx|
        client.select(1)
        info = client.client(:info)
        assert_match(info, / db=1 /, "CLIENT INFO should report db=1: #{info}")
        client.select(0)
        info = client.client(:info)
        assert_match(info, / db=0 /, "CLIENT INFO should report db=0: #{info}")
    }
end

test "SELECT is forbidden inside MULTI" do
    spawn_clients(1){|client, idx|
        assert_equal(client.multi, "OK")
        begin
            client.select(1)
            assert(false, "SELECT inside MULTI should fail")
        rescue Redis::CommandError => e
            assert_match(e.message, /SELECT is not allowed in MULTI/)
        end
        client.discard
    }
end

test "Keys keep their original cluster slot" do
    spawn_clients(1){|client, idx|
        client.select(1)
        client.set("slot:plain", "v")
        client.set("{tag}slot", "v")
        %w(slot:plain {tag}slot).each{|key|
            physical = db_test_find_physical(1, key)
            assert_not_nil(physical, "Physical key for '#{key}' not found")
            assert_equal(db_test_slot(physical), db_test_slot(key),
                         "Physical key '#{physical}' changed the slot of " +
                         "'#{key}'")
        }
        client.del("slot:plain", "{tag}slot")
        client.select(0)
    }
end

test "EVAL and RANDOMKEY work in database emulation mode" do
    spawn_clients(1){|client, idx|
        client.select(1)
        client.flushdb
        client.set("eval:key", "v")
        reply = client.eval("redis.call('SET', KEYS[1], 'x') " \
                            "return redis.call('GET', KEYS[1])", ["eval:key"])
        assert_equal(reply, "x")
        assert_equal(client.get("eval:key"), "x")
        assert_equal(client.randomkey, "eval:key")
        client.flushdb
        client.select(0)
    }
end

test "Standalone emulation" do
    spawn_clients(1){|client, idx|
        hello = client.call(["HELLO", "2"])
        fields = Hash[*hello]
        assert_equal(fields["mode"], "standalone")
        info = client.call(["INFO"])
        assert_match(info, /redis_mode:standalone/)
        assert_match(info, /cluster_enabled:0/)
        assert(!info.include?("# Keyspace"),
               "INFO should omit the keyspace section")
        [["CLUSTER", "INFO"], ["READONLY"], ["READWRITE"], ["ASKING"]].each{|cmd|
            begin
                client.call(cmd)
                assert(false, "#{cmd.join(' ')} should fail")
            rescue Redis::CommandError => e
                assert_match(e.message, /cluster support disabled/,
                             "Unexpected error for #{cmd.join(' ')}: " +
                             e.message)
            end
        }
    }
end

test "Database emulation can be disabled" do
    proxy = nil
    begin
        proxy = RedisClusterProxy.new $main_cluster,
            log_level: ($options[:log_level] || 'info'),
            disable_db_emulation: true,
            disable_standalone_emulation: true
        proxy.start
        spawn_clients(1, proxy: proxy){|client, idx|
            begin
                client.select(1)
                assert(false, "SELECT 1 should fail without DB emulation")
            rescue Redis::CommandError => e
                assert_match(e.message, /cluster mode/)
            end
            assert_equal(client.select(0), "OK")
            hello = Hash[*client.call(["HELLO", "2"])]
            assert_equal(hello["mode"], "cluster")
            assert_equal(client.call(["READONLY"]), "OK")
        }
    ensure
        proxy.stop if proxy
    end
end
