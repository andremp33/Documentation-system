#include "common.h"
#include "server.h"
#include "index.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

CacheEntry cache[MAX_CACHE];
int cache_size = 0;
int next_id = 1;
char document_folder[256] = {0};
extern void cache_print_stats();
extern void cache_export_snapshot(const char *filename);
static int debug_mode = 1;  // Debug mode flag

void send_response(const char *client_fifo, const char *response) {
    int fd = open(client_fifo, O_WRONLY);
    if (fd == -1) {
        if (debug_mode) perror("Error opening client FIFO");
        return;
    }
    
    ssize_t bytes_written = write(fd, response, strlen(response));
    if (bytes_written == -1 || (size_t)bytes_written != strlen(response)) {
        if (debug_mode) perror("Error writing to client FIFO");
    }
    close(fd);
}

void handle_add(Message *msg) {
    char title[MAX_TITLE+1] = {0}; 
    char authors[MAX_AUTHORS+1] = {0};
    char year[MAX_YEAR+1] = {0}; 
    char path[MAX_PATH+1] = {0};
    
    int fields_read = sscanf(msg->args, "%200[^|]|%200[^|]|%4[^|]|%64[^|]", 
                           title, authors, year, path);
    
    if (fields_read != 4) {
        send_response(msg->client_fifo, "Error: Invalid format for add command");
        return;
    }
    
    char fullpath[MAX_PATH + 256] = {0};
    if (snprintf(fullpath, sizeof(fullpath), "%s/%s", document_folder, path) >= sizeof(fullpath)) {
        send_response(msg->client_fifo, "Error: Path too long");
        return;
    }

    char response[RESPONSE_SIZE];
    if (access(fullpath, F_OK) == -1) {
        snprintf(response, sizeof(response), "Error: File %s not found", path);
        send_response(msg->client_fifo, response);
        return;
    }

    int id = index_add(title, authors, year, path);
    if (id > 0) {
        if (snprintf(response, sizeof(response), "Document %d indexed", id) >= sizeof(response)) {
            strncpy(response, "Document indexed", sizeof(response) - 1);
            response[sizeof(response) - 1] = '\0';
        }
        index_save("data/index.txt");
    } else {
        strncpy(response, "Error adding document", sizeof(response) - 1);
        response[sizeof(response) - 1] = '\0';
    }
    send_response(msg->client_fifo, response);
}

void handle_query(Message *msg) {
    int id = atoi(msg->args);
    DocumentMeta *doc = index_query(id);
    char response[RESPONSE_SIZE] = {0};

    if (doc) {
        int written = snprintf(response, sizeof(response),
                "Title: %s\nAuthors: %s\nYear: %s\nPath: %s",
                doc->title, doc->authors, doc->year, doc->path);
        
        if (written >= sizeof(response)) {
            if (debug_mode) fprintf(stderr, "Warning: Response truncated in query\n");
        }
    } else {
        snprintf(response, sizeof(response), "Document %d not found", id);
    }
    send_response(msg->client_fifo, response);
}

void handle_remove(Message *msg) {
    int id = atoi(msg->args);
    char response[RESPONSE_SIZE];

    if (index_remove(id) == 0) {
        snprintf(response, sizeof(response), "Index entry %d deleted", id);
        index_save("data/index.txt");
    } else {
        snprintf(response, sizeof(response), "Document %d not found", id);
    }
    send_response(msg->client_fifo, response);
}

void handle_line_count(Message *msg) {
    char keyword[128] = {0};
    int id;
    
    // Parse arguments
    char *args_copy = strdup(msg->args);
    if (!args_copy) {
        send_response(msg->client_fifo, "Error: Memory allocation failed");
        return;
    }
    
    char *token = strtok(args_copy, "|");
    if (!token) {
        free(args_copy);
        send_response(msg->client_fifo, "Error: Invalid arguments format");
        return;
    }
    
    id = atoi(token);
    token = strtok(NULL, "|");
    if (token) {
        strncpy(keyword, token, sizeof(keyword) - 1);
    }
    free(args_copy);

    DocumentMeta *doc = index_query(id);
    char response[RESPONSE_SIZE];

    if (!doc) {
        snprintf(response, sizeof(response), "Document %d not found", id);
        send_response(msg->client_fifo, response);
        return;
    }

    char fullpath[MAX_PATH + 256];
    if (snprintf(fullpath, sizeof(fullpath), "%s/%s", document_folder, doc->path) >= sizeof(fullpath)) {
        send_response(msg->client_fifo, "Error: Path too long");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        snprintf(response, sizeof(response), "Error: Pipe creation failed");
        send_response(msg->client_fifo, response);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        send_response(msg->client_fifo, "Error: Fork failed");
        return;
    }
    
    if (pid == 0) {
        // Child process
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            _exit(1);
        }
        close(pipefd[1]);
        execlp("grep", "grep", "-c", keyword, fullpath, (char *)NULL);
        _exit(1);
    } else {
        // Parent process
        close(pipefd[1]);
        char buf[64] = {0};
        ssize_t bytes_read = read(pipefd[0], buf, sizeof(buf) - 1);
        close(pipefd[0]);
        
        int status;
        waitpid(pid, &status, 0);
        
        if (bytes_read > 0) {
            snprintf(response, sizeof(response), "%s", buf);
        } else {
            snprintf(response, sizeof(response), "0");
        }
        send_response(msg->client_fifo, response);
    }
}

void handle_search(Message *msg) {
    // Safe allocation with proper checking
    char *result = NULL;
    char *keyword = NULL;
    char *nproc_str = NULL;
    int nproc = 0;
    int total = index_total();
    
    // Make a copy of the arguments for safe parsing
    char *args_copy = strdup(msg->args);
    if (!args_copy) {
        send_response(msg->client_fifo, "[]");
        return;
    }
    
    keyword = strtok(args_copy, "|");
    if (!keyword) {
        free(args_copy);
        send_response(msg->client_fifo, "[]");
        return;
    }
    
    nproc_str = strtok(NULL, "|");
    nproc = (nproc_str != NULL) ? atoi(nproc_str) : 0;
    
    // Allocate memory for result
    result = malloc(65536);
    if (!result) {
        free(args_copy);
        send_response(msg->client_fifo, "[]");
        return;
    }
    result[0] = '[';
    result[1] = '\0';

    // ---------- SEQUENTIAL MODE ----------
    if (nproc <= 0 || nproc == 1 || total <= 1) {
        int first = 1;

        for (int i = 0; i < total; i++) {
            DocumentMeta *doc = index_get(i);
            if (!doc) continue;

            char fullpath[MAX_PATH + 256];
            if (snprintf(fullpath, sizeof(fullpath), "%s/%s", document_folder, doc->path) >= sizeof(fullpath)) {
                continue;  // Skip if path is too long
            }

            int pipefd[2];
            if (pipe(pipefd) == -1) continue;
            
            pid_t pid = fork();
            if (pid == -1) {
                close(pipefd[0]);
                close(pipefd[1]);
                continue;
            }
            
            if (pid == 0) {
                // Child process
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                execlp("grep", "grep", "-q", keyword, fullpath, (char *)NULL);
                _exit(1);
            }

            // Parent process
            close(pipefd[1]);
            close(pipefd[0]);
            
            int status;
            waitpid(pid, &status, 0);
            
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                // Match found
                if (!first) strncat(result, ", ", 65536 - strlen(result) - 1);
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", doc->id);
                strncat(result, buf, 65536 - strlen(result) - 1);
                first = 0;
            }
        }

        strncat(result, "]", 65536 - strlen(result) - 1);
        send_response(msg->client_fifo, result);
        free(result);
        free(args_copy);
        return;
    }

    // ---------- CONCURRENT MODE ----------
    if (nproc > total) nproc = total;  // Limit number of processes
    
    int fds[nproc][2];
    pid_t pids[nproc];
    int docs_per_proc = total / nproc;
    int rest = total % nproc;

    for (int i = 0, start = 0; i < nproc; i++) {
        int count = docs_per_proc + (i < rest ? 1 : 0);
        if (pipe(fds[i]) == -1) {
            // Handle pipe creation error
            for (int j = 0; j < i; j++) {
                close(fds[j][0]);
                kill(pids[j], SIGTERM);
                waitpid(pids[j], NULL, 0);
            }
            send_response(msg->client_fifo, "[]");
            free(result);
            free(args_copy);
            return;
        }

        pids[i] = fork();
        if (pids[i] == -1) {
            // Handle fork error
            for (int j = 0; j < i; j++) {
                close(fds[j][0]);
                kill(pids[j], SIGTERM);
                waitpid(pids[j], NULL, 0);
            }
            close(fds[i][0]);
            close(fds[i][1]);
            send_response(msg->client_fifo, "[]");
            free(result);
            free(args_copy);
            return;
        }
        
        if (pids[i] == 0) {
            // Child process
            for (int j = 0; j < i; j++) {
                close(fds[j][0]);
                close(fds[j][1]);
            }
            close(fds[i][0]);

            char partial[4096] = "";
            int first = 1;

            for (int j = 0; j < count; j++) {
                int index = start + j;
                DocumentMeta *doc = index_get(index);
                if (!doc) continue;

                char fullpath[MAX_PATH + 256];
                if (snprintf(fullpath, sizeof(fullpath), "%s/%s", document_folder, doc->path) >= sizeof(fullpath)) {
                    continue;  // Skip if path is too long
                }

                int grep_pipe[2];
                if (pipe(grep_pipe) == -1) continue;
                
                pid_t grep_pid = fork();
                if (grep_pid == -1) {
                    close(grep_pipe[0]);
                    close(grep_pipe[1]);
                    continue;
                }
                
                if (grep_pid == 0) {
                    // Grep process
                    close(grep_pipe[0]);
                    dup2(grep_pipe[1], STDOUT_FILENO);
                    close(grep_pipe[1]);
                    execlp("grep", "grep", "-q", keyword, fullpath, (char *)NULL);
                    _exit(1);
                }
                
                // Child process handling grep result
                close(grep_pipe[1]);
                close(grep_pipe[0]);
                
                int status;
                waitpid(grep_pid, &status, 0);
                
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    // Match found
                    if (!first) strncat(partial, ", ", sizeof(partial) - strlen(partial) - 1);
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%d", doc->id);
                    strncat(partial, buf, sizeof(partial) - strlen(partial) - 1);
                    first = 0;
                }
            }

            ssize_t bytes_written = write(fds[i][1], partial, strlen(partial));
            if (bytes_written == -1 || (size_t)bytes_written != strlen(partial)) {
                if (debug_mode) perror("Write error in child process");
            }
            close(fds[i][1]);
            exit(0);
        } else {
            // Parent process
            close(fds[i][1]);
            start += count;
        }
    }

    int first = 1;
    for (int i = 0; i < nproc; i++) {
        char buffer[4096] = {0};
        ssize_t bytes_read = read(fds[i][0], buffer, sizeof(buffer) - 1);
        close(fds[i][0]);
        
        int status;
        waitpid(pids[i], &status, 0);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            if (strlen(buffer) > 0) {
                if (!first) strncat(result, ", ", 65536 - strlen(result) - 1);
                strncat(result, buffer, 65536 - strlen(result) - 1);
                first = 0;
            }
        }
    }

    strncat(result, "]", 65536 - strlen(result) - 1);
    send_response(msg->client_fifo, result);
    free(result);
    free(args_copy);
}

void handle_shutdown(Message *msg) {
    char response[RESPONSE_SIZE];
    snprintf(response, sizeof(response), "Server is shutting down");
    send_response(msg->client_fifo, response);
    index_save("data/index.txt");
    cache_print_stats();
    unlink(FIFO_SERVER);
    cache_export_snapshot("data/cache_snapshot.txt");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <document_folder> [cache_size]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Create data directory if it doesn't exist
    mkdir("data", 0777);

    if (index_load("data/index.txt") == 0) {
        printf("[INFO] Index loaded successfully.\n");
    } else {
        printf("[INFO] No index loaded.\n");
    }

    if (strlen(argv[1]) >= sizeof(document_folder)) {
        fprintf(stderr, "Error: Document folder path too long\n");
        return EXIT_FAILURE;
    }
    strncpy(document_folder, argv[1], sizeof(document_folder) - 1);
    document_folder[sizeof(document_folder) - 1] = '\0';
    
    // Create document folder if it doesn't exist
    if (mkdir(document_folder, 0777) == -1 && errno != EEXIST) {
        perror("mkdir document folder");
        return EXIT_FAILURE;
    }

    if (argc >= 3) {
        cache_size = atoi(argv[2]);
        if (cache_size > MAX_CACHE) cache_size = MAX_CACHE;
        if (cache_size <= 0) cache_size = 10; // Default value
    }

    unlink(FIFO_SERVER);
    if (mkfifo(FIFO_SERVER, 0666) == -1) {
        perror("mkfifo");
        return EXIT_FAILURE;
    }

    printf("Server started. Document folder: %s\n", document_folder);
    printf("Loaded %d documents. Cache size: %d\n", index_get_count(), cache_size);

    int fd = open(FIFO_SERVER, O_RDWR);
    if (fd == -1) {
        perror("open FIFO");
        unlink(FIFO_SERVER);
        return EXIT_FAILURE;
    }

    Message msg;
    while (1) {
        ssize_t bytes = read(fd, &msg, sizeof(msg));
        if (bytes <= 0) continue;
        
        if (bytes != sizeof(msg)) {
            if (debug_mode) fprintf(stderr, "Warning: Incomplete message received\n");
            continue;
        }

        // Ensure null-termination of strings
        msg.client_fifo[sizeof(msg.client_fifo) - 1] = '\0';
        msg.args[sizeof(msg.args) - 1] = '\0';

        switch (msg.command) {
            case CMD_ADD: handle_add(&msg); break;
            case CMD_QUERY: handle_query(&msg); break;
            case CMD_REMOVE: handle_remove(&msg); break;
            case CMD_LINE_COUNT: handle_line_count(&msg); break;
            case CMD_SEARCH: handle_search(&msg); break;
            case CMD_SHUTDOWN: handle_shutdown(&msg); break;
            default:
                if (debug_mode) fprintf(stderr, "Unknown command: %d\n", msg.command);
                send_response(msg.client_fifo, "Error: Unknown command");
                break;
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}