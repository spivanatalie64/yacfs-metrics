/*
 * yacfs-metrics — Prometheus metrics exporter for YAcFS pools
 *
 * Exposes pool statistics via HTTP in Prometheus text format.
 * Usage: yacfs-metrics <pool_root> [port]
 *
 * Metrics:
 *   yacfs_inodes_total        Number of inodes in pool
 *   yacfs_blocks_total        Number of block files in pool
 *   yacfs_snapshots_total     Number of snapshots in pool
 *   yacfs_block_bytes         Total size of block storage
 *   yacfs_meta_bytes          Total size of metadata storage
 */

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct pool_stats {
    int inodes;
    int blocks;
    int snapshots;
    unsigned long block_bytes;
    unsigned long meta_bytes;
};

static int get_stats(const char *pool, struct pool_stats *stats) {
    memset(stats, 0, sizeof(*stats));
    char path[4096];
    struct stat sb;

    snprintf(path, sizeof(path), "%s/meta", pool);
    if (stat(path, &sb) == 0) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (strstr(de->d_name, ".ino")) stats->inodes++;
            }
            closedir(d);
        }
        stats->meta_bytes = sb.st_size;
    }

    snprintf(path, sizeof(path), "%s/blocks", pool);
    if (stat(path, &sb) == 0) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (strstr(de->d_name, ".blk")) stats->blocks++;
            }
            closedir(d);
        }
        stats->block_bytes = sb.st_size;
    }

    snprintf(path, sizeof(path), "%s/.snapshots", pool);
    if (stat(path, &sb) == 0) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] != '.') stats->snapshots++;
            }
            closedir(d);
        }
    }

    return 0;
}

static void serve_metrics(int client_fd, const char *pool) {
    struct pool_stats stats;
    get_stats(pool, &stats);

    char buf[8192];
    int n = snprintf(buf, sizeof(buf),
        "# HELP yacfs_inodes_total Number of inodes in the pool\n"
        "# TYPE yacfs_inodes_total gauge\n"
        "yacfs_inodes_total{pool=\"%s\"} %d\n"
        "\n"
        "# HELP yacfs_blocks_total Number of block files in the pool\n"
        "# TYPE yacfs_blocks_total gauge\n"
        "yacfs_blocks_total{pool=\"%s\"} %d\n"
        "\n"
        "# HELP yacfs_snapshots_total Number of snapshots\n"
        "# TYPE yacfs_snapshots_total gauge\n"
        "yacfs_snapshots_total{pool=\"%s\"} %d\n"
        "\n"
        "# HELP yacfs_block_bytes Total size of block storage\n"
        "# TYPE yacfs_block_bytes gauge\n"
        "yacfs_block_bytes{pool=\"%s\"} %lu\n"
        "\n"
        "# HELP yacfs_meta_bytes Total size of metadata storage\n"
        "# TYPE yacfs_meta_bytes gauge\n"
        "yacfs_meta_bytes{pool=\"%s\"} %lu\n",
        pool, stats.inodes,
        pool, stats.blocks,
        pool, stats.snapshots,
        pool, stats.block_bytes,
        pool, stats.meta_bytes);

    char response[9216];
    int rn = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n%s", n, buf);

    write(client_fd, response, rn);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pool_root> [port]\n", argv[0]);
        fprintf(stderr, "Exposes Prometheus metrics via HTTP on port 9101 (default).\n");
        return 1;
    }

    const char *pool = argv[1];
    int port = argc > 2 ? atoi(argv[2]) : 9101;

    struct stat sb;
    if (stat(pool, &sb) < 0) {
        fprintf(stderr, "Pool not found: %s\n", pool);
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        fprintf(stderr, "Try: %s %s %d\n", argv[0], pool, port);
        close(server_fd);
        return 1;
    }

    listen(server_fd, 5);
    printf("YAcFS Metrics: %s on :%d/metrics\n", pool, port);

    while (1) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int client_fd = accept(server_fd, (struct sockaddr*)&client, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        char req[4096];
        ssize_t n = read(client_fd, req, sizeof(req) - 1);
        if (n > 0) {
            req[n] = 0;
            if (strstr(req, "GET /metrics"))
                serve_metrics(client_fd, pool);
            else {
                const char *bad = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
                write(client_fd, bad, strlen(bad));
            }
        }
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
