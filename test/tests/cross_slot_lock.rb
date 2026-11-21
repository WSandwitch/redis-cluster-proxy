require 'redis'
require 'hiredis'

setup {
    RedisProxyTestCase::GenericSetup.call
    loglevel = $options[:log_level] || 'info'
    use_valgrind = $options[:valgrind] == true
    # Valgrind makes everything much slower, so use a larger lock timeout.
    $lock_timeout = use_valgrind ? 2000 : 400
    $lock_ttl = use_valgrind ? 10000 : 2000
    $lock_proxy = RedisClusterProxy.new $main_cluster,
        log_level: loglevel,
        threads: 2,
        cross_slot_write_policy: 'lock',
        cross_slot_lock_timeout: $lock_timeout,
        cross_slot_lock_ttl: $lock_ttl,
        valgrind: use_valgrind
    $lock_proxy.start
    $lock_proxy2 = RedisClusterProxy.new $main_cluster,
        log_level: loglevel,
        threads: 2,
        cross_slot_write_policy: 'lock',
        cross_slot_lock_timeout: $lock_timeout,
        cross_slot_lock_ttl: $lock_ttl,
        valgrind: use_valgrind
    $lock_proxy2.start
    $error_proxy = RedisClusterProxy.new $main_cluster,
        log_level: loglevel,
        threads: 1,
        cross_slot_write_policy: 'error',
        valgrind: use_valgrind
    $error_proxy.start
}

cleanup {
    $lock_proxy.stop
    $lock_proxy = nil
    $lock_proxy2.stop
    $lock_proxy2 = nil
    $error_proxy.stop
    $error_proxy = nil
}

# Return true if the cross-slot lock key exists on any node.
def lock_test_exists?
    $main_cluster.masters.any?{|node|
        begin
            Redis.new(port: node[:port]).exists("{__proxy:lock}")
        rescue Redis::CommandError
            false
        end
    }
end

# Set the cross-slot lock key directly on the node that owns it (ie. as if
# another proxy was holding it).
def lock_test_hold_external(ttl: nil)
    ttl ||= ($lock_timeout * 4)
    $main_cluster.masters.each{|node|
        begin
            return Redis.new(port: node[:port]).
                set("{__proxy:lock}", "external-holder", nx: true, px: ttl)
        rescue Redis::CommandError => e
            next if e.message.start_with?("MOVED")
            raise
        end
    }
    nil
end

def lock_test_release_external
    $main_cluster.masters.each{|node|
        begin
            Redis.new(port: node[:port]).del("{__proxy:lock}")
        rescue Redis::CommandError
            next
        end
    }
end

def lock_test_wait_released(timeout: 3)
    deadline = Time.now + timeout
    while Time.now < deadline
        return true if !lock_test_exists?
        sleep 0.05
    end
    !lock_test_exists?
end

test "Cross-slot writes acquire and release the lock" do
    spawn_clients(1, proxy: $lock_proxy){|client, idx|
        assert_equal(client.mset("lock:a", "1", "lock:b", "2"), "OK")
        assert_equal(client.mget("lock:a", "lock:b"), %w(1 2))
    }
    assert(lock_test_wait_released,
           "The cross-slot lock key was not released")
    $lock_proxy.del("lock:a", "lock:b")
end

test "An externally held lock makes cross-slot writes time out" do
    assert(lock_test_hold_external, "Failed to hold the lock externally")
    begin
        spawn_clients(1, proxy: $lock_proxy){|client, idx|
            err = nil
            begin
                client.mset("lock:x", "1", "lock:y", "2")
            rescue Redis::CommandError => e
                err = e
            end
            assert_not_nil(err, "Expected the cross-slot write to fail")
            assert_match(err.message, /lock acquisition timed out/,
                         "Unexpected error: #{err.message}")
            # Single-key operations must not be blocked by the lock.
            assert_equal(client.set("lock:single", "ok"), "OK")
            assert_equal(client.get("lock:single"), "ok")
        }
    ensure
        lock_test_release_external
    end
    spawn_clients(1, proxy: $lock_proxy){|client, idx|
        assert_equal(client.mset("lock:x", "1", "lock:y", "2"), "OK")
        $lock_proxy.del("lock:x", "lock:y", "lock:single")
    }
end

test "Cross-slot writes are serialized between proxies" do
    iterations = 100
    errors = []
    mutex = Mutex.new
    writers = [["A", $lock_proxy], ["B", $lock_proxy2]].map{|val, proxy|
        Thread.new{
            begin
                r = Redis.new port: proxy.port
                iterations.times{
                    reply = r.mset("conc:a", val, "conc:b", val)
                    if reply != "OK"
                        mutex.synchronize{ errors << reply }
                    end
                }
            rescue Exception => e
                mutex.synchronize{ errors << e }
            ensure
                r.close if r
            end
        }
    }
    writers.each{|t| t.join}
    # In lock mode cross-slot writes are serialized, so no timeout is
    # expected (the timeout is well above a single write).
    assert(errors.empty?, "Unexpected errors: #{errors.first(3).inspect}")
    assert(lock_test_wait_released,
           "The cross-slot lock key was not released")
    spawn_clients(1, proxy: $lock_proxy){|client, idx|
        values = client.mget("conc:a", "conc:b")
        assert_equal(values[0], values[1],
                     "Cross-slot write was applied partially: #{values.inspect}")
        $lock_proxy.del("conc:a", "conc:b")
    }
end

test "Cross-slot write policy 'error' rejects writes but allows reads" do
    spawn_clients(1, proxy: $error_proxy){|client, idx|
        err = nil
        begin
            client.mset("err:a", "1", "err:b", "2")
        rescue Redis::CommandError => e
            err = e
        end
        assert_not_nil(err, "Expected the cross-slot write to be rejected")
        assert_match(err.message, /CROSSSLOT/)
        # Cross-slot reads are still allowed.
        assert_class(client.mget("err:a", "err:b"), Array)
        # Same-slot writes are allowed.
        assert_equal(client.mset("{tag}a", "1", "{tag}b", "2"), "OK")
        client.del("{tag}a", "{tag}b")
    }
end
