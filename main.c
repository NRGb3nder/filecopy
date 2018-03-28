#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include "utils.h"

#define MIN_VALID_ARGC 4
#define MIN_RUNNING_PROC 2
#define CHAR_BUF_SIZE 256

int fcopy(const char *source, const char *dest, long process_count);
int ensure_semop(int sem_id, struct sembuf *op);
int copy_part(const char *source, const char *dest, int dest_fd, size_t bufsize,
    int sem_id, off_t part_offset, size_t part_size);
int write_s(const char *dest, int dest_fd, const char *outbuf, size_t outsize, off_t part_offset, int sem_id);

char *module;
struct sembuf op_lock = {.sem_op = -1};
struct sembuf op_unlock = {.sem_op = 1};

int main(int argc, char *argv[]) {
    module = basename(argv[0]);

    if (argc < MIN_VALID_ARGC) {
        printerr(module, "Too few arguments", NULL);
        return 1;
    }

    if (!isreg(argv[1])) {
        printerr(module, strerror(errno), argv[1]);
        return 1;
    }

    if (!access(argv[2], F_OK)) {
        printerr(module, "File already exists", argv[2]);
        return 1;
    }

    long process_count;
    if (!(process_count = strtol(argv[3], NULL, 10))) {
        printerr(module, "Number of processes is not an integer", NULL);
        return 1;
    }
    if (errno == ERANGE) {
        printerr(module, strerror(errno), NULL);
        return 1;
    }
    if (process_count < MIN_RUNNING_PROC) {
        char errmsg[CHAR_BUF_SIZE];
        sprintf(errmsg, "Number of processes must be greater or equal to %d",
                MIN_RUNNING_PROC);

        printerr(module, errmsg, NULL);
        return 1;
    }

    char source[PATH_MAX];
    char dest[PATH_MAX];
    realpath(argv[1], source);
    realpath(argv[2], dest);

    return fcopy(source, dest, process_count);
}

int fcopy(const char *source, const char *dest, long process_count)
{
    int sem_id;
    if (sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0666), sem_id == -1) {
        printerr(module, strerror(errno), "semget");
        return 1;
    }

    if (ensure_semop(sem_id, &op_unlock) == -1) {
        printerr(module, strerror(errno), "semop");
        return 1;
    }

    struct stat statbuf;
    if (lstat(source, &statbuf) == -1) {
        printerr(module, strerror(errno), source);
        return 1;
    }

    int dest_fd;
    if (dest_fd = open(dest, O_CREAT | O_EXCL | O_WRONLY, statbuf.st_mode), dest_fd == -1) {
        printerr(module, strerror(errno), dest);
        return 1;
    }

    size_t part_size = (size_t) statbuf.st_size / process_count;
    off_t part_offset = 0;

    pid_t child;
    for (int i = 1; i <= process_count; i++) {
        if (child = fork(), child == -1) {
            printerr(module, strerror(errno), "fork");
            return 1;
        }

        if (!child) {
            return copy_part(source, dest, dest_fd, (size_t) statbuf.st_blksize,
                sem_id, part_offset, i != process_count ? part_size : (size_t) (statbuf.st_size - part_offset));
        } else {
            part_offset += part_size;
        }
    }

    while (wait(NULL) != -1) {
        /* block */
    }

    if (close(dest_fd) == -1) {
        printerr(module, strerror(errno), dest);
        return 1;
    }

    return 0;
}

int ensure_semop(int sem_id, struct sembuf *op)
{
    int result;
    do {
        result = semop(sem_id, op, 1);
    } while (result == -1 && errno == EINTR);

    return result;
}

int copy_part(const char *source, const char *dest, int dest_fd, size_t bufsize,
    int sem_id, off_t part_offset, size_t part_size)
{
    off_t part_end = part_offset + part_size;
    char buf[bufsize];

    int source_fd;
    if (source_fd = open(source, O_RDONLY), source_fd == -1) {
        printerr(module, strerror(errno), source);
        return 1;
    }

    bool is_seekerror = false;
    if (lseek(source_fd, part_offset, SEEK_SET) == -1) {
        printerr(module, strerror(errno), source);
        is_seekerror = true;
    }

    bool is_rdwrerror = false;
    if (!is_seekerror) {
        while (!is_rdwrerror && part_offset < part_end) {
            ssize_t bytes_count;
            bool is_last_chunk = part_end - part_offset <= bufsize;
            if (bytes_count = read(source_fd, buf, is_last_chunk ? (size_t) (part_end - part_offset) : bufsize), bytes_count == -1) {
                printerr(module, strerror(errno), source);
                is_rdwrerror = true;
            }
            if (!is_rdwrerror) {
                if (write_s(dest, dest_fd, buf, (size_t) bytes_count, part_offset, sem_id) == -1) {
                    printerr(module, strerror(errno), dest);
                    is_rdwrerror = true;
                }
                part_offset += bytes_count;
            }
        }
    }

    if (close(source_fd) == -1) {
        printerr(module, strerror(errno), source);
        return 1;
    }

    return is_seekerror || is_rdwrerror;
}

int write_s(const char *dest, int dest_fd, const char *outbuf, size_t outsize, off_t part_offset, int sem_id)
{
    if (ensure_semop(sem_id, &op_lock) == -1) {
        printerr(module, strerror(errno), "locking semaphore");
        return -1;
    }

    if (lseek(dest_fd, part_offset, SEEK_SET) == -1) {
        printerr(module, strerror(errno), dest);
        return -1;
    }
    if (write(dest_fd, outbuf, outsize) == -1) {
        printerr(module, strerror(errno), dest);
        return -1;
    }

    if (ensure_semop(sem_id, &op_unlock) == -1) {
        printerr(module, strerror(errno), "unlocking semaphore");
        return -1;
    }

    return 0;
}
