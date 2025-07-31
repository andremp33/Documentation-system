#include "common.h"
#include "client.h"

void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s -a \"title\" \"authors\" \"year\" \"path\"\n", prog);
    fprintf(stderr, "  %s -c \"key\"\n", prog);
    fprintf(stderr, "  %s -d \"key\"\n", prog);
    fprintf(stderr, "  %s -l \"key\" \"keyword\"\n", prog);
    fprintf(stderr, "  %s -s \"keyword\" [nr_processes]\n", prog);
    fprintf(stderr, "  %s -f\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc < 2) usage(argv[0]);

    Message msg;
    memset(&msg, 0, sizeof(msg));

    // Create unique FIFO for responses
    char client_fifo[256];
    if (snprintf(client_fifo, sizeof(client_fifo), "/tmp/docindex_%d_fifo", getpid()) >= sizeof(client_fifo)) {
        fprintf(stderr, "Error: client FIFO path too long\n");
        exit(EXIT_FAILURE);
    }
    strncpy(msg.client_fifo, client_fifo, sizeof(msg.client_fifo) - 1);
    msg.client_fifo[sizeof(msg.client_fifo) - 1] = '\0';
    
    if (mkfifo(client_fifo, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    // Parse command
    if (strcmp(argv[1], "-a") == 0 && argc == 6) {
        msg.command = CMD_ADD;
        if (snprintf(msg.args, sizeof(msg.args), "%s|%s|%s|%s", 
                argv[2], argv[3], argv[4], argv[5]) >= sizeof(msg.args)) {
            fprintf(stderr, "Error: Arguments too long\n");
            unlink(client_fifo);
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(argv[1], "-c") == 0 && argc == 3) {
        msg.command = CMD_QUERY;
        if (snprintf(msg.args, sizeof(msg.args), "%s", argv[2]) >= sizeof(msg.args)) {
            fprintf(stderr, "Error: Key too long\n");
            unlink(client_fifo);
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(argv[1], "-d") == 0 && argc == 3) {
        msg.command = CMD_REMOVE;
        if (snprintf(msg.args, sizeof(msg.args), "%s", argv[2]) >= sizeof(msg.args)) {
            fprintf(stderr, "Error: Key too long\n");
            unlink(client_fifo);
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(argv[1], "-l") == 0 && argc == 4) {
        msg.command = CMD_LINE_COUNT;
        if (snprintf(msg.args, sizeof(msg.args), "%s|%s", argv[2], argv[3]) >= sizeof(msg.args)) {
            fprintf(stderr, "Error: Arguments too long\n");
            unlink(client_fifo);
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(argv[1], "-s") == 0 && (argc == 3 || argc == 4)) {
        msg.command = CMD_SEARCH;
        if (snprintf(msg.args, sizeof(msg.args), argc == 4 ? "%s|%s" : "%s", 
                argv[2], argc == 4 ? argv[3] : "") >= sizeof(msg.args)) {
            fprintf(stderr, "Error: Arguments too long\n");
            unlink(client_fifo);
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(argv[1], "-f") == 0 && argc == 2) {
        msg.command = CMD_SHUTDOWN;
    } else {
        usage(argv[0]);
    }

    // Send request
    int fd = open(FIFO_SERVER, O_WRONLY);
    if (fd == -1) {
        perror("open server FIFO");
        unlink(client_fifo);
        exit(EXIT_FAILURE);
    }
    
    ssize_t bytes_written = write(fd, &msg, sizeof(msg));
    if (bytes_written != sizeof(msg)) {
        perror("write to server FIFO");
        close(fd);
        unlink(client_fifo);
        exit(EXIT_FAILURE);
    }
    close(fd);

    // Get response
    fd = open(client_fifo, O_RDONLY);
    if (fd == -1) {
        perror("open client FIFO");
        unlink(client_fifo);
        exit(EXIT_FAILURE);
    }

    char response[RESPONSE_SIZE];
    ssize_t n = read(fd, response, sizeof(response) - 1);
    if (n > 0) {
        response[n] = '\0';
        printf("%s\n", response);
    } else if (n == 0) {
        fprintf(stderr, "Error: Empty response from server\n");
    } else {
        perror("read from client FIFO");
    }
    
    close(fd);
    unlink(client_fifo);
    return 0;
}