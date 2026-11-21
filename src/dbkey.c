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
 */

#include <string.h>
#include <ctype.h>
#include "dbkey.h"
#include "db_tags.h"
#include "cluster.h"
#include "config.h"

#define DB_PREFIX_MAX_LEN 64

int dbEmulationEnabled(void) {
    return config.db_prefix != NULL && config.db_prefix[0] != '\0';
}

sds dbNumberPrefix(int db) {
    if (!dbEmulationEnabled()) return sdsempty();
    return sdscatprintf(sdsempty(), config.db_prefix, db);
}

sds dbPhysicalKey(int db, const char *key, size_t len) {
    if (!dbEmulationEnabled())
        return sdsnewlen(key, len);
    unsigned int slot = clusterKeyHashSlot((char *) key, (int) len);
    if (slot >= DB_SLOT_TAGS_COUNT) return NULL;
    sds out = dbNumberPrefix(db);
    if (out == NULL) return NULL;
    out = sdscatlen(out, "{", 1);
    out = sdscatlen(out, DB_SLOT_TAGS[slot], DB_SLOT_TAG_LEN);
    out = sdscatlen(out, "}", 1);
    out = sdscatlen(out, key, len);
    return out;
}

sds dbStripKey(int db, const char *key, size_t len) {
    if (!dbEmulationEnabled()) return sdsnewlen(key, len);
    sds prefix = dbNumberPrefix(db);
    if (prefix == NULL) return NULL;
    size_t plen = sdslen(prefix);
    size_t head = plen + 1 + DB_SLOT_TAG_LEN + 1; /* prefix + {tag} */
    if (len < head || memcmp(key, prefix, plen) != 0 ||
        key[plen] != '{' ||
        key[plen + 1 + DB_SLOT_TAG_LEN] != '}')
    {
        sdsfree(prefix);
        return NULL;
    }
    const char *logical = key + head;
    size_t logical_len = len - head;
    /* Verify that the tag is the one expected for the logical key: keys that
     * do not follow the layout (eg. written directly into Redis, or belonging
     * to another convention) must not be exposed to clients. */
    unsigned int slot = clusterKeyHashSlot((char *) logical, (int) logical_len);
    if (slot >= DB_SLOT_TAGS_COUNT ||
        memcmp(key + plen + 1, DB_SLOT_TAGS[slot], DB_SLOT_TAG_LEN) != 0)
    {
        sdsfree(prefix);
        return NULL;
    }
    sdsfree(prefix);
    return sdsnewlen(logical, logical_len);
}

sds dbKeyPattern(int db, const char *pattern, size_t len) {
    if (!dbEmulationEnabled()) return sdsnewlen(pattern, len);
    sds out = dbNumberPrefix(db);
    if (out == NULL) return NULL;
    out = sdscatlen(out, "{", 1);
    int i;
    for (i = 0; i < DB_SLOT_TAG_LEN; i++) out = sdscatlen(out, "?", 1);
    out = sdscatlen(out, "}", 1);
    out = sdscatlen(out, pattern, len);
    return out;
}

int dbValidatePrefix(const char *prefix, const char **err) {
    const char *p;
    int conversions = 0;
    if (prefix == NULL || prefix[0] == '\0') {
        if (err != NULL) *err = NULL;
        return 1;
    }
    if (strlen(prefix) > DB_PREFIX_MAX_LEN) {
        if (err != NULL) *err = "DB key prefix is too long";
        return 0;
    }
    for (p = prefix; *p; p++) {
        if (*p == '%') {
            if (p[1] != 'd') {
                if (err != NULL)
                    *err = "DB key prefix must contain exactly one '%d' "
                           "conversion";
                return 0;
            }
            conversions++;
            p++;
        } else if (*p == '{' || *p == '}' || *p == '*' || *p == '?' ||
                   *p == '[' || *p == ']' || *p == '\\')
        {
            if (err != NULL)
                *err = "DB key prefix cannot contain '{', '}', '*', '?', "
                       "'[', ']' or '\\'";
            return 0;
        } else if (*p == '%') {
            if (err != NULL) *err = "Invalid DB key prefix";
            return 0;
        }
    }
    if (conversions != 1) {
        if (err != NULL)
            *err = "DB key prefix must contain exactly one '%d' conversion";
        return 0;
    }
    return 1;
}

int dbSelfCheck(void) {
    int i, j;
    for (i = 0; i < DB_SLOT_TAGS_COUNT; i++) {
        const char *tag = DB_SLOT_TAGS[i];
        if (strlen(tag) != DB_SLOT_TAG_LEN) return 0;
        for (j = 0; j < DB_SLOT_TAG_LEN; j++)
            if (!isalnum((unsigned char) tag[j])) return 0;
        if ((int) clusterKeyHashSlot((char *) tag, DB_SLOT_TAG_LEN) != i)
            return 0;
    }
    return 1;
}
