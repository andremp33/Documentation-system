#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#define FIFO_SERVER "/tmp/docindex_server_fifo"
#define MAX_DOCUMENTS 2500
#define MAX_TITLE 200
#define MAX_AUTHORS 200
#define MAX_YEAR 4
#define MAX_PATH 64
#define MAX_KEY 16
#define RESPONSE_SIZE 1024
#define MAX_CACHE 500

typedef enum {
    CMD_ADD,
    CMD_QUERY,
    CMD_REMOVE,
    CMD_LINE_COUNT,
    CMD_SEARCH,
    CMD_SHUTDOWN
} CommandType;

typedef struct {
    int id;
    char title[MAX_TITLE+1];
    char authors[MAX_AUTHORS+1];
    char year[MAX_YEAR+1];
    char path[MAX_PATH+1];
} DocumentMeta;

typedef struct {
    CommandType command;
    char client_fifo[256];
    char args[512];
} Message;

typedef struct {
    int id;
    DocumentMeta meta;
} CacheEntry;

#endif