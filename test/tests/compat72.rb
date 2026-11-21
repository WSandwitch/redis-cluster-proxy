require 'socket'
require 'redis'
require 'hiredis'

setup &RedisProxyTestCase::GenericSetup

$options ||= {}

def compat_raw_connect
    TCPSocket.new('127.0.0.1', $main_proxy.port)
end

def compat_raw_format(*args)
    out = "*#{args.length}\r\n"
    args.each{|arg|
        arg = arg.to_s
        out << "$#{arg.bytesize}\r\n#{arg}\r\n"
    }
    out
end

def compat_raw_read(sock)
    line = sock.gets("\r\n")
    raise "Unexpected EOF from proxy" if line.nil?
    type = line[0]
    body = line[1..-3]
    case type
    when '+', '-', ':'
        return line[0..-3]
    when '$'
        len = body.to_i
        return nil if len < 0
        data = sock.read(len)
        sock.read(2)
        return data
    when '*'
        len = body.to_i
        return nil if len < 0
        return (0...len).map{ compat_raw_read(sock) }
    else
        raise "Unexpected reply type '#{type}'"
    end
end

def compat_raw_command(*args)
    sock = compat_raw_connect
    sock.write(compat_raw_format(*args))
    reply = compat_raw_read(sock)
    sock.close
    reply
end

test "Empty bulk arguments are accepted" do
    spawn_clients(1){|client, idx|
        reply = client.set "compat:empty", ""
        assert_equal(reply, "OK")
        reply = client.get "compat:empty"
        assert_equal(reply, "")
        # Verify at byte level too: SET compat:empty ''
        reply = compat_raw_command("SET", "compat:empty2", "")
        assert_equal(reply, "+OK")
        reply = compat_raw_command("GET", "compat:empty2")
        assert_equal(reply, "")
    }
end

test "Empty multibulk queries are ignored" do
    sock = compat_raw_connect
    # '*0' and '*-1' must be silently ignored, then PING must be answered.
    sock.write("*0\r\n*-1\r\n" + compat_raw_format("PING"))
    reply = compat_raw_read(sock)
    assert_equal(reply, "+PONG")
    sock.close
end

test "EVAL does not treat ARGV as a key" do
    spawn_clients(1){|client, idx|
        reply = client.eval("return ARGV[1]", ["compat:evalkey"],
                            ["compat:argv"])
        assert_equal(reply, "compat:argv")
        # Keys are prefixed by database emulation, so scripts must use
        # KEYS[i] (never build key names from ARGV).
        reply = client.eval(
            "redis.call('SET', KEYS[1], 'v') " \
            "return redis.call('GET', KEYS[1])",
            ["compat:evalkey"])
        assert_equal(reply, "v")
        reply = client.get "compat:evalkey"
        assert_equal(reply, "v")
        # No keys, only ARGV: must not be treated as a key for routing.
        reply = client.eval("return ARGV[1]", [], ["compat:argv"])
        assert_equal(reply, "compat:argv")
    }
end

test "Cross-slot MGET/MSET/DEL are enabled by default" do
    spawn_clients(1){|client, idx|
        keys = ["compat:a", "compat:b", "{compat}c"]
        reply = client.mget(*keys)
        assert_class(reply, Array)
        assert_equal(reply.length, keys.length)
        reply = client.mset(*keys.zip(%w(1 2 3)).flatten)
        assert_equal(reply, "OK")
        reply = client.mget(*keys)
        assert_equal(reply, %w(1 2 3))
        reply = client.del(*keys)
        assert_equal(reply, keys.length)
    }
end

test "Pipeline replies preserve order" do
    spawn_clients(1){|client, idx|
        replies = client.pipelined{|pipeline|
            pipeline.ping
            pipeline.set "compat:p1", "v1"
            pipeline.set "compat:p2", "v2"
            pipeline.mget "compat:p1", "compat:p2"
            pipeline.ping
            pipeline.del "compat:p1", "compat:p2"
        }
        assert_equal(replies.length, 6)
        assert_equal(replies[0], "PONG")
        assert_equal(replies[1], "OK")
        assert_equal(replies[2], "OK")
        assert_equal(replies[3], %w(v1 v2))
        assert_equal(replies[4], "PONG")
        assert_equal(replies[5], 2)
    }
end

test "RANDOMKEY never hangs" do
    spawn_clients(1){|client, idx|
        client.set "compat:randomkey", "1"
        100.times{
            reply = client.randomkey
            assert(reply.nil? || reply.is_a?(String),
                   "Unexpected RANDOMKEY reply: #{reply.inspect}")
        }
    }
end

test "HELLO 2 and HELLO 3" do
    reply = compat_raw_command("HELLO")
    assert_class(reply, Array)
    assert_equal(reply[0], "server")
    assert_equal(reply[1], "redis")

    reply = compat_raw_command("HELLO", "2")
    assert_class(reply, Array)
    assert(reply.include?("proto"), "Missing 'proto' in HELLO reply")

    sock = compat_raw_connect
    sock.write(compat_raw_format("HELLO", "3"))
    reply = compat_raw_read(sock)
    sock.close
    assert_match(reply, /NOPROTO/, "Expected NOPROTO error, got #{reply}")
end

test "CLIENT SETNAME/GETNAME/ID" do
    sock = compat_raw_connect
    sock.write(compat_raw_format("CLIENT", "SETNAME", "compat-client") +
               compat_raw_format("CLIENT", "GETNAME") +
               compat_raw_format("CLIENT", "ID") +
               compat_raw_format("CLIENT", "SETNAME", "") +
               compat_raw_format("CLIENT", "GETNAME"))
    assert_equal(compat_raw_read(sock), "+OK")
    assert_equal(compat_raw_read(sock), "compat-client")
    id = compat_raw_read(sock)
    assert_match(id, /^:\d+$/, "Unexpected CLIENT ID reply: #{id}")
    assert_equal(compat_raw_read(sock), "+OK")
    assert_nil(compat_raw_read(sock), "Empty CLIENT SETNAME should reset name")
    sock.close
end

test "SELECT: database emulation" do
    assert_equal(compat_raw_command("SELECT", "0"), "+OK")
    assert_equal(compat_raw_command("SELECT", "1"), "+OK")
    reply = compat_raw_command("SELECT", "16")
    assert_match(reply, /DB index is out of range/,
                 "Unexpected SELECT reply: #{reply}")
    reply = compat_raw_command("SELECT", "abc")
    assert_match(reply, /value is not an integer or out of range/,
                 "Unexpected SELECT reply: #{reply}")
    assert_equal(compat_raw_command("SELECT", "0"), "+OK")
end

test "INFO contains real node info and proxy sections" do
    reply = compat_raw_command("INFO")
    assert_not_nil(reply)
    assert_match(reply, /redis_version:/, "INFO misses redis_version")
    assert_match(reply, /# Proxy/, "INFO misses the proxy section")
end

test "QUIT replies OK and closes the connection" do
    sock = compat_raw_connect
    sock.write(compat_raw_format("QUIT"))
    reply = compat_raw_read(sock)
    assert_equal(reply, "+OK")
    assert_nil(sock.gets("\r\n"), "Connection should be closed after QUIT")
    sock.close
end
