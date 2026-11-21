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


#ifndef __REDIS_CLUSTER_PROXY_COMMANDS_H__
#define __REDIS_CLUSTER_PROXY_COMMANDS_H__

#include <stdlib.h>

#define PROXY_COMMAND_HANDLED      1
#define PROXY_COMMAND_UNHANDLED    0

#define PROXY_REPLY_UNHANDLED      2

/* Layout of the key names contained in a command's reply. Used by database
 * emulation to strip the database prefix before replying to the client. */
#define REPLYKEYS_NONE             0
/* Array of keys (eg. KEYS). */
#define REPLYKEYS_ARRAY            1
/* A single bulk key (eg. RANDOMKEY). */
#define REPLYKEYS_BULK             2
/* [cursor, [keys]] (eg. SCAN). */
#define REPLYKEYS_SCAN             3
/* [key, value] (eg. BLPOP, BRPOP, BZPOPMIN, BZPOPMAX). */
#define REPLYKEYS_KEY_VALUE        4
/* [key, [values]] (eg. LMPOP, ZMPOP, BLMPOP, BZMPOP). */
#define REPLYKEYS_KEY_ARRAY        5
/* [[stream, entries...], ...] (eg. XREAD, XREADGROUP). */
#define REPLYKEYS_STREAMS          6

#define CMDFLAG_MULTISLOT_UNSUPPORTED 1 << 0
#define CMDFLAG_DUPLICATE 1 << 1
#define CMDFLAG_HANDLE_REPLY 1 << 2
/* Route commands without keys to the first master node, even when they take
 * arguments (eg. LASTSAVE, TIME, ROLE). */
#define CMDFLAG_FIRST_MASTER 1 << 3

typedef int redisClusterProxyCommandHandler(void *request);
typedef int redisClusterProxyReplyHandler(void *reply, void *request,
   char *buf, int len);

/* Callback used to get key indices in some special commands, returns
 * the number of keys or -1 if some error occurs.
 * Arguments:
 *     *req: pointer to the request
 *     *first_key, *last_key, *key_step: pointers to keys indices/step
 *     **skip: pointer to an array of indices (must be allocated and
 *             freed outside) that must be skipped.
 *     *skiplen: used to indicate the length of *skip array
 *     **err: used for an eventual error message (when -1 is returned)
 */
typedef int redisClusterGetKeysCallback(void *req, int *first_key,
   int *last_key, int *key_step, int **skip, int *skiplen, char **err);


typedef struct redisCommandDef {
    char *name;
    int arity;
    int first_key;
    int last_key;
    int key_step;
    int proxy_flags;
    int unsupported;
    redisClusterGetKeysCallback     *get_keys;
    redisClusterProxyCommandHandler *handle;
    redisClusterProxyReplyHandler   *handleReply;
    /* Layout of the key names contained in the reply (see REPLYKEYS_*): it is
     * used to strip the database prefix when database emulation is enabled. */
    int reply_keys;
} redisCommandDef;


extern struct redisCommandDef redisCommandTable[245];

#endif /* __REDIS_CLUSTER_PROXY_COMMANDS_H__  */
