/* AudioScope by BerryStudio 0.7-alpha — append-only JSONL session log. */
#include "core/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

struct AsSession {
    FILE *file;
    char path[1024];
    char id[96];
    int count;
};

static int mkdir_one(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

static void mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            mkdir_one(tmp);
            *p = sep;
        }
    }
    mkdir_one(tmp);
}

static void iso_now(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

AsSession *as_session_create(const char *dir, const char *session_id) {
    if (!dir || !dir[0]) dir = ".";
    mkdir_p(dir);

    AsSession *s = (AsSession *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    snprintf(s->id, sizeof(s->id), "%s",
             (session_id && session_id[0]) ? session_id : "alpha");
    snprintf(s->path, sizeof(s->path), "%s/session.jsonl", dir);
    s->file = fopen(s->path, "ab");
    if (!s->file) {
        free(s);
        return NULL;
    }
    return s;
}

void as_session_destroy(AsSession *session) {
    if (!session) return;
    if (session->file) fclose(session->file);
    free(session);
}

int as_session_event(AsSession *s, const char *type, const char *data_json) {
    if (!s || !s->file || !type || !type[0]) return AS_ERR_INVALID;
    if (!data_json || !data_json[0]) data_json = "{}";
    char ts[32];
    iso_now(ts, sizeof(ts));
    int ok = fprintf(s->file,
                     "{\"ts\":\"%s\",\"session\":\"%s\",\"type\":\"%s\",\"data\":%s}\n",
                     ts, s->id, type, data_json) > 0;
    if (fflush(s->file) != 0) ok = 0;
    if (!ok) return AS_ERR_IO;
    ++s->count;
    return AS_OK;
}

const char *as_session_path(const AsSession *session) {
    return session ? session->path : "";
}

int as_session_count(const AsSession *session) {
    return session ? session->count : 0;
}
