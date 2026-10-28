#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEFAULT_BATCH 5000
#define TIMEOUT_MS 1000
#define MAX_PORT 65535

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int start_connect(struct in_addr ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = ip;

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }
    return fd;
}

static int fit_batch(int wanted)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0) return 500;

    rlim_t need = (rlim_t)wanted + 50;
    if (rl.rlim_cur < need) {
        rl.rlim_cur = (need < rl.rlim_max) ? need : rl.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rl);
        getrlimit(RLIMIT_NOFILE, &rl);
    }

    long usable = (long)rl.rlim_cur - 50;
    if (usable < 1) usable = 1;
    return usable < wanted ? (int)usable : wanted;
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-b batch] <ip>\n", prog);
}

int main(int argc, char **argv)
{
    int requested = DEFAULT_BATCH;
    const char *ip_str = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 2;
            }
            char *end;
            long v = strtol(argv[++i], &end, 10);
            if (*end != '\0' || v < 1) {
                fprintf(stderr, "invalid batch size\n");
                return 2;
            }
            requested = v > MAX_PORT ? MAX_PORT : (int)v;
        } else if (!ip_str) {
            ip_str = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!ip_str) {
        usage(argv[0]);
        return 2;
    }

    struct in_addr ip;
    if (inet_pton(AF_INET, ip_str, &ip) != 1) {
        fprintf(stderr, "invalid ip\n");
        return 2;
    }

    int batch = fit_batch(requested);
    if (batch < requested)
        fprintf(stderr, "batch reduced to %d (fd limit)\n", batch);

    struct pollfd *fds = malloc(sizeof(struct pollfd) * batch);
    int *ports = malloc(sizeof(int) * batch);
    int *open_ports = malloc(sizeof(int) * MAX_PORT);
    if (!fds || !ports || !open_ports) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    int open_count = 0;

    for (int start = 1; start <= MAX_PORT; start += batch) {
        int n = 0;

        for (int p = start; p < start + batch && p <= MAX_PORT; p++) {
            int fd = start_connect(ip, p);
            if (fd < 0) {
                if (errno == EMFILE || errno == ENFILE)
                    fprintf(stderr, "out of file descriptors at port %d\n", p);
                continue;
            }
            fds[n].fd = fd;
            fds[n].events = POLLOUT;
            fds[n].revents = 0;
            ports[n] = p;
            n++;
        }

        int pending = n;
        long deadline = now_ms() + TIMEOUT_MS;

        while (pending > 0) {
            long left = deadline - now_ms();
            if (left <= 0) break;
            if (poll(fds, n, (int)left) <= 0) break;

            for (int i = 0; i < n; i++) {
                if (fds[i].fd < 0 || !fds[i].revents) continue;

                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0)
                    open_ports[open_count++] = ports[i];

                close(fds[i].fd);
                fds[i].fd = -1;
                pending--;
            }
        }

        for (int i = 0; i < n; i++) {
            if (fds[i].fd >= 0) close(fds[i].fd);
        }
    }

    for (int i = 0; i < open_count - 1; i++) {
        for (int j = i + 1; j < open_count; j++) {
            if (open_ports[j] < open_ports[i]) {
                int t = open_ports[i];
                open_ports[i] = open_ports[j];
                open_ports[j] = t;
            }
        }
    }

    printf("%s -> [", ip_str);
    for (int i = 0; i < open_count; i++)
        printf(i ? ",%d" : "%d", open_ports[i]);
    printf("]\n");

    printf("nmap -sV -T4 %s -p ", ip_str);
    for (int i = 0; i < open_count; i++)
        printf(i ? ",%d" : "%d", open_ports[i]);

    free(fds);
    free(ports);
    free(open_ports);
    return 0;
}
