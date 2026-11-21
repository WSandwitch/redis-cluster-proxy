/*
 * Copyright (C) 2019  Giuseppe Fabio Nicotra <artix2 at gmail dot com>
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
 *
 * Database emulation helpers.
 *
 * A Redis Cluster only has DB 0, while clients of a standalone Redis can use
 * the SELECT command to choose one of many databases.  To emulate standalone
 * databases the proxy prefixes every key with ``<db>/<tag>``, where ``<tag>``
 * is a 3 character hash tag chosen so that the key keeps its original cluster
 * slot:
 *
 *     CRC16(tag) % 16384 == slot(original key)
 *
 * Keeping the slot means that the same key in two different databases lives
 * on the same cluster node, so MOVE and COPY between databases are
 * single-slot operations and can be executed atomically by Redis itself.
 */

#ifndef __REDIS_CLUSTER_PROXY_DBKEY_H__
#define __REDIS_CLUSTER_PROXY_DBKEY_H__

#include <stddef.h>
#include "sds.h"

#define DB_MAX_DATABASES 1024
#define DB_KEY_SLOTS 16384

/* Return 1 if database emulation is enabled (ie. a non-empty key prefix has
 * been configured). */
int dbEmulationEnabled(void);

/* Return the prefix for the given database (eg. "1/"). */
sds dbNumberPrefix(int db);

/* Return the physical key for `key` in database `db`, or NULL on allocation
 * failure. */
sds dbPhysicalKey(int db, const char *key, size_t len);

/* Return the logical key (ie. without the database prefix and hash tag) for
 * the physical `key` of database `db`, or NULL if `key` does not belong to
 * that database (or on allocation failure). */
sds dbStripKey(int db, const char *key, size_t len);

/* Return a glob pattern matching the logical keys of database `db` that match
 * the given logical `pattern` (eg. "1/" -> "1/{???}<pattern>"). */
sds dbKeyPattern(int db, const char *pattern, size_t len);

/* Validate a configured key prefix. Return 1 if the prefix is valid,
 * elsewhere 0 and set *err to a static error message. */
int dbValidatePrefix(const char *prefix, const char **err);

/* Verify the generated hash tag table (see db_tags.h). Return 1 on success. */
int dbSelfCheck(void);

#endif /* __REDIS_CLUSTER_PROXY_DBKEY_H__ */
