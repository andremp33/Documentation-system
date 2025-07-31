#include "common.h"
#include "index.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static DocumentMeta docs[MAX_DOCUMENTS];
static int doc_count = 0;
static int next_id = 1;

// LRU Cache
static CacheEntry cache[MAX_CACHE];
static int cache_count = 0;
extern int cache_size;

int debug_mode = DEBUG_MODE;
static int cache_hits = 0;
static int cache_misses = 0;


void cache_print_stats() {
    if (debug_mode) {
        printf("[CACHE] Stats → Hits: %d, Misses: %d, Total: %d\n",
               cache_hits, cache_misses, cache_hits + cache_misses);
    }
}


void cache_move_to_front(int index) {
    if (index <= 0 || index >= cache_count) return;
    CacheEntry temp = cache[index];
    for (int i = index; i > 0; i--) {
        cache[i] = cache[i - 1];
    }
    cache[0] = temp;
    if (debug_mode) printf("[CACHE] ID %d movido para o topo (LRU)\n", temp.id);
}

void cache_add(int id, DocumentMeta *doc) {
    for (int i = 0; i < cache_count; i++) {
        if (cache[i].id == id) {
            if (debug_mode) printf("[CACHE] ID %d já está na cache — não adicionado novamente\n", id);
            return;
        }
    }

    if (cache_count == cache_size) {
        if (debug_mode) printf("[CACHE] Removido ID %d (mais antigo)\n", cache[cache_count - 1].id);
        cache_count--;
    }

    for (int i = cache_count; i > 0; i--) {
        cache[i] = cache[i - 1];
    }

    cache[0].id = id;
    memcpy(&cache[0].meta, doc, sizeof(DocumentMeta));
    cache_count++;

    if (debug_mode) printf("[CACHE] ID %d adicionado\n", id);
}


DocumentMeta* index_query(int id) {
    for (int i = 0; i < cache_count; i++) {
        if (cache[i].id == id) {
            if (debug_mode) printf("[CACHE] HIT: ID %d\n", id);
            cache_hits++;
            cache_move_to_front(i);
            return &cache[0].meta;
        }
    }

    if (debug_mode) printf("[CACHE] MISS: ID %d\n", id);
    cache_misses++;

    for (int i = 0; i < doc_count; i++) {
        if (docs[i].id == id) {
            cache_add(id, &docs[i]);
            return &docs[i];
        }
    }

    return NULL;
}


void cache_export_snapshot(const char *filename) {
    if (!filename) return;
    
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        if (debug_mode) perror("[CACHE] Error creating snapshot file");
        return;
    }

    char line[512];

    int header_len = snprintf(line, sizeof(line), "Cache Snapshot - %d entries\n", cache_count);
    if (write(fd, line, header_len) == -1) {
        if (debug_mode) perror("[CACHE] Error writing header");
        close(fd);
        return;
    }
    
    for (int i = 0; i < cache_count; i++) {
        int len = snprintf(line, sizeof(line), "ID %d: %s\n", cache[i].id, cache[i].meta.title);
        write(fd, line, len);
    }

    close(fd);
    if (debug_mode) printf("[CACHE] Snapshot exportado para %s\n", filename);
}


int index_add(const char *title, const char *authors, const char *year, const char *path) {
    if (doc_count >= MAX_DOCUMENTS) return -1;

    docs[doc_count].id = next_id++;

    char real_title[MAX_TITLE + 1] = "Desconhecido";
    char real_author[MAX_AUTHORS + 1] = "Desconhecido";
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", document_folder, path);
    extract_metadata(fullpath, real_title, sizeof(real_title), real_author, sizeof(real_author));

    strncpy(docs[doc_count].title, real_title, MAX_TITLE);
    strncpy(docs[doc_count].authors, real_author, MAX_AUTHORS);
    strncpy(docs[doc_count].year, year, MAX_YEAR);
    strncpy(docs[doc_count].path, path, MAX_PATH);

    doc_count++;
    return docs[doc_count - 1].id;
}

int index_remove(int id) {
    for (int i = 0; i < doc_count; i++) {
        if (docs[i].id == id) {
            for (int j = i; j < doc_count - 1; j++) {
                docs[j] = docs[j + 1];
            }
            doc_count--;
            return 0;
        }
    }
    return -1;
}

int index_load(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    doc_count = 0;
    next_id = 1;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        int id;
        char title[MAX_TITLE+1], authors[MAX_AUTHORS+1], year[MAX_YEAR+1], path[MAX_PATH+1];

        if (sscanf(line, "%d|%200[^|]|%200[^|]|%4[^|]|%64[^\n]",
                   &id, title, authors, year, path) == 5) {

            docs[doc_count].id = id;
            strncpy(docs[doc_count].title, title, MAX_TITLE);
            strncpy(docs[doc_count].authors, authors, MAX_AUTHORS);
            strncpy(docs[doc_count].year, year, MAX_YEAR);
            strncpy(docs[doc_count].path, path, MAX_PATH);

            if (id >= next_id) next_id = id + 1;
            doc_count++;
            if (doc_count >= MAX_DOCUMENTS) break;
        }
    }

    fclose(fp);
    return 1;
}

int extract_metadata(const char *filepath, char *title, size_t max_title, char *author, size_t max_author) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return -1;

    char line[512];
    int found_title = 0, found_author = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (!found_title && strncmp(line, "Title:", 6) == 0) {
            strncpy(title, line + 7, max_title - 1);
            title[strcspn(title, "\r\n")] = '\0';
            found_title = 1;
        } else if (!found_author && strncmp(line, "Author:", 7) == 0) {
            strncpy(author, line + 8, max_author - 1);
            author[strcspn(author, "\r\n")] = '\0';
            found_author = 1;
        }
        if (found_title && found_author) break;
    }

    fclose(fp);
    if (!found_title) strncpy(title, "Desconhecido", max_title);
    if (!found_author) strncpy(author, "Desconhecido", max_author);
    return 0;
}

int index_save(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) return 0;

    char buffer[1024];
    for (int i = 0; i < doc_count; i++) {
        int len = snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s|%s\n",
                           docs[i].id,
                           docs[i].title,
                           docs[i].authors,
                           docs[i].year,
                           docs[i].path);
        write(fd, buffer, len);
    }

    close(fd);
    return 1;
}

int index_total() {
    return doc_count;
}

DocumentMeta* index_get(int i) {
    if (i >= 0 && i < doc_count) return &docs[i];
    return NULL;
}

int index_get_count() {
    return doc_count;
}