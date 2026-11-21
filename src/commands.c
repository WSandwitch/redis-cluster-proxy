/*
 * Copyright (C) 2019-2020  Giuseppe Fabio Nicotra <artix2 at gmail dot com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "commands.h"

/* Command Handlers */
int proxyCommand(void *req);
int multiCommand(void *req);
int execOrDiscardCommand(void *req);
int commandWithPrivateConnection(void *req);
int xreadCommand(void *req);
int securityWarningCommand(void *req);
int pingCommand(void *req);
int authCommand(void *req);
int scanCommand(void *req);
int selectCommand(void *req);
int quitCommand(void *req);
int okCommand(void *req);
int resetCommand(void *req);
int helloCommand(void *req);
int clientCommand(void *req);
int configCommand(void *req);
int scriptCommand(void *req);
int keysCommand(void *req);
int randomkeyCommand(void *req);
int clusterCommand(void *req);
int readOnlyCommand(void *req);
int dbsizeCommand(void *req);
int dbsizeReply(void *reply, void *req, char *buf, int len);
int flushdbCommand(void *req);
int flushdbReply(void *reply, void *req, char *buf, int len);
int moveCommand(void *req);
int copyCommand(void *req);
int swapdbCommand(void *req);
int sortCommand(void *req);

/* Reply Handlers */
int mergeReplies(void *reply, void *request, char *buf, int len);
int getFirstMultipleReply(void *reply, void *request, char *buf, int len);
int sumReplies(void *reply, void *request, char *buf, int len);
int handleScanReply(void *reply, void *request, char *buf, int len);
int getRandomReply(void *reply, void *request, char *buf, int len);
int getInfoReply(void *reply, void *request, char *buf, int len);

/* Get Keys Callbacks */
int zunionInterGetKeys(void *req, int *first_key, int *last_key,
   int *key_step, int **skip, int *skiplen, char **err);
int xreadGetKeys(void *req, int *first_key, int *last_key,
   int *key_step, int **skip, int *skiplen, char **err);
int sortGetKeys(void *req, int *first_key, int *last_key,
   int *key_step, int **skip, int *skiplen, char **err);
int evalGetKeys(void *req, int *first_key, int *last_key,
   int *key_step, int **skip, int *skiplen, char **err);
int numkeysGetKeys(void *req, int *first_key, int *last_key,
   int *key_step, int **skip, int *skiplen, char **err);

struct redisCommandDef redisCommandTable[245] = {
    {"bitpos", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"cluster", -2, 0, 0, 0, 0, 0, NULL, clusterCommand, NULL},
    {"randomkey", 1, 0, 0, 0,
     CMDFLAG_HANDLE_REPLY,
     0, NULL, randomkeyCommand, getRandomReply, REPLYKEYS_BULK},
    {"georadius", -6, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sdiff", -2, 1, -1, 1, 0, 0, NULL, NULL, NULL},
    {"flushdb", -1, 0, 0, 0,
     CMDFLAG_DUPLICATE | CMDFLAG_HANDLE_REPLY,
     0, NULL, flushdbCommand, flushdbReply},
    {"pfmerge", -2, 1, -1, 1, 0, 0, NULL, NULL, NULL},
    {"strlen", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"shutdown", -1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"hello", -1, 0, 0, 0, 0, 0, NULL, helloCommand, NULL},
    {"hincrby", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"multi", 1, 0, 0, 0, 0, 0, NULL, multiCommand, NULL},
    {"script", -2, 0, 0, 0, 0, 0, NULL, scriptCommand, getFirstMultipleReply},
    {"unwatch", 1, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, commandWithPrivateConnection, getFirstMultipleReply},
    {"setbit", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"srem", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lrem", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"getbit", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lpop", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"bzpopmin", -3, 1, -2, 1, 0, 0, NULL, commandWithPrivateConnection, NULL, REPLYKEYS_KEY_VALUE},
    {"set", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"persist", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"module", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"monitor", 1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"geohash", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"psubscribe", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"hget", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"psetex", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"eval", -3, 0, 0, 0, 0, 0, evalGetKeys, NULL, NULL},
    {"rename", 3, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"dump", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"pubsub", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"unsubscribe", -1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"slowlog", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"smove", 4, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"xdel", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"ttl", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zincrby", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"msetnx", -3, 1, -1, 2, 0, 0, NULL, NULL, NULL},
    {"rpoplpush", 3, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"setrange", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"bgrewriteaof", 1, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, NULL, getFirstMultipleReply},
    {"bitcount", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"getset", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"llen", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zrange", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"xinfo", -2, 2, 2, 1, 0, 0, NULL, NULL, NULL},
    {"zremrangebyscore", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"config", -2, 0, 0, 0, 0, 0, NULL, configCommand, getFirstMultipleReply},
    {"mget", -2, 1, -1, 1, 0, 0, NULL, NULL, mergeReplies},
    {"host:", -1, 0, 0, 0, 0, 0, NULL, securityWarningCommand, NULL},
    {"restore", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zlexcount", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hdel", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"brpoplpush", 4, 1, 2, 1, 0, 0, NULL, commandWithPrivateConnection, NULL},
    {"sismember", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"bzpopmax", -3, 1, -2, 1, 0, 0, NULL, commandWithPrivateConnection, NULL, REPLYKEYS_KEY_VALUE},
    {"sscan", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"unlink", -2, 1, -1, 1, 0, 0, NULL, NULL, sumReplies},
    {"hsetnx", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"substr", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hscan", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"watch", -2, 1, -1, 1, 0, 0, NULL, commandWithPrivateConnection, getFirstMultipleReply},
    {"append", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"xadd", -5, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sinter", -2, 1, -1, 1, 0, 0, NULL, NULL, NULL},
    {"slaveof", 3, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"zpopmin", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lolwut", -1, 0, 0, 0, 0, 0, NULL, NULL, getFirstMultipleReply},
    {"xack", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"get", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hmset", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"dbsize", 1, 0, 0, 0,
     CMDFLAG_HANDLE_REPLY,
     0, NULL, dbsizeCommand, dbsizeReply},
    {"sync", 1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"xgroup", -2, 2, 2, 1, 0, 0, NULL, NULL, NULL},
    {"pfcount", -2, 1, -1, 1, 0, 0, NULL, NULL, NULL},
    {"georadiusbymember_ro", -5, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hmget", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"geodist", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"replicaof", 3, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"auth", -2, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, authCommand, getFirstMultipleReply},
    {"incrbyfloat", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"info", -1, 0, 0, 0,
     CMDFLAG_HANDLE_REPLY,
     0, NULL, NULL, getInfoReply},
    {"lpush", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"select", 2, 0, 0, 0, 0, 0, NULL, selectCommand, NULL},
    {"pfadd", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hkeys", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sinterstore", -3, 1, -1, 1,
     CMDFLAG_MULTISLOT_UNSUPPORTED,
     0, NULL, NULL, NULL},
    {"migrate", -6, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"rpushx", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"pfdebug", 3, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"command", -1, 0, 0, 0, 0, 0, NULL, NULL, getFirstMultipleReply},
    {"xpending", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"spop", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"echo", 2, 0, 0, 0, 0, 0, NULL, NULL, getFirstMultipleReply},
    {"exec", 1, 0, 0, 0, 0, 0, NULL, execOrDiscardCommand, NULL},
    {"geoadd", -5, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"readwrite", 1, 0, 0, 0, 0, 0, NULL, readOnlyCommand, NULL},
    {"touch", -2, 1, -1, 1, 0, 0, NULL, NULL, sumReplies},
    {"expireat", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zinterstore", -4, 0, 0, 0,
     CMDFLAG_MULTISLOT_UNSUPPORTED,
     0, zunionInterGetKeys, NULL, NULL},
    {"ltrim", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"xtrim", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"move", 3, 1, 1, 1, 0, 0, NULL, moveCommand, NULL},
    {"object", -2, 2, 2, 1, 0, 0, NULL, NULL, NULL},
    {"zpopmax", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zcount", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hset", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"pexpireat", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zrem", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"ping", -1, 0, 0, 0, 0, 0, NULL, pingCommand, NULL},
    {"zrevrangebylex", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"flushall", -1, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, NULL, getFirstMultipleReply},
    {"subscribe", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"evalsha", -3, 0, 0, 0, 0, 0, evalGetKeys, NULL, NULL},
    {"zremrangebyrank", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"publish", 3, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"zrevrangebyscore", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"swapdb", 3, 0, 0, 0, 0, 0, NULL, swapdbCommand, NULL},
    {"latency", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"zscore", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lset", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"scan", -2, 0, 0, 0,
     CMDFLAG_HANDLE_REPLY,
     0, NULL, scanCommand, handleScanReply, REPLYKEYS_SCAN},
    {"debug", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"zrevrank", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"asking", 1, 0, 0, 0, 0, 0, NULL, readOnlyCommand, NULL},
    {"zremrangebylex", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"georadiusbymember", -5, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hlen", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"renamenx", 3, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"acl", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"hgetall", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"incr", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"punsubscribe", -1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"setnx", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"del", -2, 1, -1, 1, 0, 0, NULL, NULL, sumReplies},
    {"xrange", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sunionstore", -3, 1, -1, 1,
     CMDFLAG_MULTISLOT_UNSUPPORTED,
     0, NULL, NULL, NULL},
    {"pfselftest", 1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"smembers", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"bitop", -4, 2, -1, 1, 0, 0, NULL, NULL, NULL},
    {"zrank", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"keys", 2, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, keysCommand, mergeReplies, REPLYKEYS_ARRAY},
    {"pttl", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"xlen", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"georadius_ro", -6, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"xread", -4, 1, 1, 1,
     CMDFLAG_MULTISLOT_UNSUPPORTED,
     0, xreadGetKeys, xreadCommand, NULL, REPLYKEYS_STREAMS},
    {"sunion", -2, 1, -1, 1, 0, 0, NULL, NULL, NULL},
    {"psync", -3, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"xrevrange", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lrange", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"xreadgroup", -7, 1, 1, 1,
     CMDFLAG_MULTISLOT_UNSUPPORTED,
     0, xreadGetKeys, xreadCommand, NULL, REPLYKEYS_STREAMS},
    {"zcard", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"rpop", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hstrlen", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lastsave", 1, 0, 0, 0,
     CMDFLAG_FIRST_MASTER,
     0, NULL, NULL, NULL},
    {"setex", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"brpop", -3, 1, -2, 1, 0, 0, NULL, commandWithPrivateConnection, NULL, REPLYKEYS_KEY_VALUE},
    {"time", 1, 0, 0, 0,
     CMDFLAG_FIRST_MASTER,
     0, NULL, NULL, NULL},
    {"zunionstore", -4, 0, 0, 0,
     CMDFLAG_MULTISLOT_UNSUPPORTED,
     0, zunionInterGetKeys, NULL, NULL},
    {"scard", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"role", 1, 0, 0, 0,
     CMDFLAG_FIRST_MASTER,
     0, NULL, NULL, NULL},
    {"expire", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sadd", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sdiffstore", -3, 1, -1, 1,
     CMDFLAG_MULTISLOT_UNSUPPORTED,
     0, NULL, NULL, NULL},
    {"post", -1, 0, 0, 0, 0, 0, NULL, securityWarningCommand, NULL},
    {"hincrbyfloat", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hvals", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zscan", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"geopos", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"bitfield", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"decrby", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"replconf", -1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"wait", 3, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"rpush", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"blpop", -3, 1, -2, 1, 0, 0, NULL, commandWithPrivateConnection, NULL, REPLYKEYS_KEY_VALUE},
    {"zadd", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hexists", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"save", 1, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, NULL, getFirstMultipleReply},
    {"type", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"restore-asking", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zrevrange", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zrangebyscore", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"incrby", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"mset", -3, 1, -1, 2, 0, 0, NULL, NULL, getFirstMultipleReply},
    {"discard", 1, 0, 0, 0, 0, 0, NULL, execOrDiscardCommand, NULL},
    {"xclaim", -6, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"decr", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"getrange", 4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"xsetid", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lindex", 3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"readonly", 1, 0, 0, 0, 0, 0, NULL, readOnlyCommand, NULL},
    {"bgsave", -1, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, NULL, getFirstMultipleReply},
    {"sort", -2, 1, 1, 1, 0, 0, sortGetKeys, sortCommand, NULL},
    {"srandmember", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zrangebylex", -4, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"linsert", 5, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lpushx", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"client", -2, 0, 0, 0, 0, 0, NULL, clientCommand, NULL},
    {"memory", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"exists", -2, 1, -1, 1, 0, 0, NULL, NULL, sumReplies},
    {"pexpire", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"quit", -1, 0, 0, 0, 0, 0, NULL, quitCommand, NULL},
    {"reset", 1, 0, 0, 0, 0, 0, NULL, resetCommand, NULL},
    /* Redis 7.2 commands */
    {"bitfield_ro", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"blmove", 6, 1, 2, 1, 0, 0, NULL, commandWithPrivateConnection, NULL},
    {"blmpop", -5, 0, 0, 0, 0, 0, evalGetKeys,
     commandWithPrivateConnection, NULL, REPLYKEYS_KEY_ARRAY},
    {"bzmpop", -5, 0, 0, 0, 0, 0, evalGetKeys,
     commandWithPrivateConnection, NULL, REPLYKEYS_KEY_ARRAY},
    {"copy", -3, 1, 2, 1, 0, 0, NULL, copyCommand, NULL},
    {"eval_ro", -3, 0, 0, 0, 0, 0, evalGetKeys, NULL, NULL},
    {"evalsha_ro", -3, 0, 0, 0, 0, 0, evalGetKeys, NULL, NULL},
    {"expiretime", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"fcall", -3, 0, 0, 0, 0, 0, evalGetKeys, NULL, NULL},
    {"fcall_ro", -3, 0, 0, 0, 0, 0, evalGetKeys, NULL, NULL},
    {"function", -2, 0, 0, 0,
     CMDFLAG_DUPLICATE,
     0, NULL, NULL, getFirstMultipleReply},
    {"geosearch", -7, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"geosearchstore", -8, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"getdel", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"getex", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"hrandfield", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"lcs", -3, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"lmove", 5, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"lmpop", -4, 0, 0, 0, 0, 0, numkeysGetKeys, NULL, NULL, REPLYKEYS_KEY_ARRAY},
    {"lpos", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"pexpiretime", 2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sintercard", -3, 0, 0, 0, 0, 0, numkeysGetKeys, NULL, NULL},
    {"smismember", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"sort_ro", -2, 1, 1, 1, 0, 0, sortGetKeys, NULL, NULL},
    {"xautoclaim", -6, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zdiff", -3, 0, 0, 0, 0, 0, numkeysGetKeys, NULL, NULL},
    {"zdiffstore", -4, 0, 0, 0, 0, 0, zunionInterGetKeys, NULL, NULL},
    {"zinter", -3, 0, 0, 0, 0, 0, numkeysGetKeys, NULL, NULL},
    {"zintercard", -3, 0, 0, 0, 0, 0, numkeysGetKeys, NULL, NULL},
    {"zmpop", -4, 0, 0, 0, 0, 0, numkeysGetKeys, NULL, NULL, REPLYKEYS_KEY_ARRAY},
    {"zmscore", -3, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zrandmember", -2, 1, 1, 1, 0, 0, NULL, NULL, NULL},
    {"zrangestore", -5, 1, 2, 1, 0, 0, NULL, NULL, NULL},
    {"zunion", -3, 0, 0, 0, 0, 0, numkeysGetKeys, NULL, NULL},
    {"failover", -1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"sentinel", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"spublish", 3, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"ssubscribe", -2, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"sunsubscribe", -1, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    {"waitaof", 4, 0, 0, 0, 0, 1, NULL, NULL, NULL},
    /* Custom Commands */
    {"proxy", -2, 0, 0, 0, 0, 0, NULL, proxyCommand, NULL}
};

/* Redis commands having the WRITE flag (generated from the Redis 7.2 command
 * specifications). Used to decide whether a cross-slot query is a write when
 * the cross-slot write policy is "error" or "lock". Sorted for bsearch
 * (well, for the plain binary search below). */
static const char *WRITE_COMMANDS[] = {
    "append", "bitfield", "bitop", "blmove", "blmpop", "blpop", "brpop",
    "brpoplpush", "bzmpop", "bzpopmax", "bzpopmin", "copy", "decr", "decrby",
    "del", "expire", "expireat", "flushall", "flushdb", "geoadd", "georadius",
    "georadiusbymember", "geosearchstore", "getdel", "getex", "getset",
    "hdel", "hincrby", "hincrbyfloat", "hmset", "hset", "hsetnx", "incr",
    "incrby", "incrbyfloat", "linsert", "lmove", "lmpop", "lpop", "lpush",
    "lpushx", "lrem", "lset", "ltrim", "migrate", "move", "mset", "msetnx",
    "persist", "pexpire", "pexpireat", "pfadd", "pfdebug", "pfmerge",
    "psetex", "rename", "renamenx", "restore", "rpop", "rpoplpush", "rpush",
    "rpushx", "sadd", "sdiffstore", "set", "setbit", "setex", "setnx",
    "setrange", "sinterstore", "smove", "sort", "spop", "srem", "sunionstore",
    "swapdb", "unlink", "xack", "xadd", "xautoclaim", "xclaim", "xdel",
    "xreadgroup", "xsetid", "xtrim", "zadd", "zdiffstore", "zincrby",
    "zinterstore", "zmpop", "zpopmax", "zpopmin", "zrangestore", "zrem",
    "zremrangebylex", "zremrangebyrank", "zremrangebyscore", "zunionstore",
};

#define WRITE_COMMANDS_COUNT 98

int commandIsWrite(const char *name) {
    int lo = 0, hi = WRITE_COMMANDS_COUNT - 1;
    if (name == NULL) return 0;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcmp(name, WRITE_COMMANDS[mid]);
        if (cmp == 0) return 1;
        if (cmp < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return 0;
}
