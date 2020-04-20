#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <unistd.h>
#include <wait.h>
#include <pthread.h>
#include <sys/mman.h>

extern const char *const opname[256];

pthread_mutex_t *mutex;
static long long *code_usages;

void
emit_report(int event, int frame, int opcode) {
    if (mutex) {
        if (event == 1) {
            pthread_mutex_lock(mutex);
            code_usages[opcode]++;
            pthread_mutex_unlock(mutex);
        }
    }
}

static void
dump_report(const char *filepath) {
    FILE *file = fopen(filepath, "wt");
    if (!file) {
        perror(NULL);
        raise(SIGKILL);
    }

    long long total_usages = 0;
    for (int i = 0; i < 256; ++i) {
        total_usages += code_usages[i];
    }
    int visited[256] = {};
    while (1) {
        int max_index = 0;
        long long max_freq = 0;
        for (int i = 0; i < 256; i++) {
            if (!visited[i] && code_usages[i] > max_freq) {
                max_index = i;
                max_freq = code_usages[i];
            }
        }
        visited[max_index] = 1;
        if (max_freq) {
            fprintf(file, "%s: %lld (%.3f%%)\n",
                    opname[max_index],
                    max_freq,
                    max_freq * 100 / (double) total_usages
            );
        } else {
            break;
        }
    }
    fclose(file);
}

static void
finish(int sig) {
    signal(sig, finish);
}

void
start_report() {
    const char *report = getenv("REPORT");
    if (report && report[0] != '\0') {
        int unsetenv(const char *);
        unsetenv("REPORT");
    } else {
        return;
    }

    void *shared_mem = mmap(NULL,
                            sizeof(pthread_mutex_t) + sizeof(long long) * 256,
                            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
                            -1, 0);
    if (shared_mem == MAP_FAILED) {
        perror(NULL);
        raise(SIGKILL);
    }

    mutex = shared_mem;
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(mutex, &mutexattr);

    code_usages = shared_mem + sizeof(pthread_mutex_t);

    pid_t pid = fork();
    if (pid < 0) {
        perror(NULL);
        raise(SIGKILL);
    } else if (pid > 0) {
        signal(SIGINT, finish);
        signal(SIGTERM, finish);

        int status;
        waitpid(pid, &status, 0);
        dump_report(report);

        if (WIFEXITED(status)) {
            exit(WEXITSTATUS(status));
        } else {
            signal(SIGINT, NULL);
            signal(SIGTERM, NULL);
            raise(WTERMSIG(status));
        }
    }
}
