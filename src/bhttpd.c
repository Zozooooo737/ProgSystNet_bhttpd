#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 26130
#define BUFFER_SIZE  4096

#define REQUEST_REGEX "^(GET|HEAD|POST) (/[^ \r\n]*) HTTP/1\\.[01](\r?\n|$)"

static void usage(FILE *out, const char *prog) {
    fprintf(out,
        "Usage: %s [-c file] [-d] [-h] [-l file] [-p port] [-s file]\n"
        "  -c <file>  chemin du fichier de configuration\n"
        "  -d         active le mode de debogage\n"
        "  -h         affiche cette aide et quitte\n"
        "  -l <file>  chemin du fichier de logs\n"
        "  -p <port>  port d'ecoute (1-65535)\n"
        "  -s <file>  chemin du fichier socket\n",
        prog);
}

static void send_response(int fd, const char *status, const char *body, int send_body) {
    char date[64];
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", &tm_utc);

    char response[2048];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.0 %s\r\n"
        "Date: %s\r\n"
        "Server: bhttpd/0.1\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        status, date, strlen(body), send_body ? body : "");
    if (len < 0 || (size_t)len >= sizeof(response)) {
        return;
    }
    send(fd, response, (size_t)len, MSG_NOSIGNAL);
}

static void handle_request(int fd, int debug) {
    char buffer[BUFFER_SIZE];
    ssize_t n = recv(fd, buffer, BUFFER_SIZE - 1, 0);
    if (n <= 0) {
        return;
    }
    buffer[n] = '\0';
    if (debug) {
        fprintf(stderr, "[debug] requete recue :\n%s\n", buffer);
    }

    regex_t re;
    if (regcomp(&re, REQUEST_REGEX, REG_EXTENDED) != 0) {
        fprintf(stderr, "regcomp failed\n");
        return; 
    }

    regmatch_t m[3];
    if (regexec(&re, buffer, 3, m, 0) == REG_NOMATCH) {
        if (debug) {
            fprintf(stderr, "[debug] requete invalide -> 400\n");
        }
        send_response(fd, "400 Bad Request",
            "<title>400 Bad Request</title>\n"
            "<h1>400 Bad Request</h1>\n"
            "<p>Requete invalide.</p>\n"
            "<hr>\n"
            "<p>Enzo BOTTURA - BIC 2</p>\n", 1);
    } else {
        char method[8] = {0};
        char path[256] = {0};
        snprintf(method, sizeof(method), "%.*s" ,(int)(m[1].rm_eo - m[1].rm_so), buffer + m[1].rm_so);
        snprintf(path, sizeof(path), "%.*s", (int)(m[2].rm_eo - m[2].rm_so), buffer + m[2].rm_so);
        if (debug) {
            fprintf(stderr, "[debug] methode=%s chemin=%s\n", method, path);
        }
        send_response(fd, "200 OK",
            "<title>200 OK</title>\n"
            "<h1>200 OK</h1>\n"
            "<hr>\n"
            "<p>Hello World!</p>\n"
            "<p>Enzo BOTTURA - BIC 2</p>\n",
            strcmp(method, "HEAD") != 0);
    }
    regfree(&re);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    int debug = 0;
    const char *conffile = NULL;
    const char *logfile = NULL;
    const char *sockfile = NULL;
    int opt;

    opterr = 0;
    while ((opt = getopt(argc, argv, "c:dhl:p:s:")) != -1) {
        switch (opt) {
        case 'c':
            conffile = optarg;
            break;
        case 'd':
            debug = 1;
            break;
        case 'h':
            usage(stdout, argv[0]);
            return EXIT_SUCCESS;
        case 'l':
            logfile = optarg;
            break;
        case 'p': {
            char *end;
            errno = 0;
            long v = strtol(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0' || v < 1 || v > 65535) {
                fprintf(stderr, "Invalid argument: %s\n", optarg);
                return EXIT_FAILURE;
            }
            port = (int)v;
            break;
        }
        case 's':
            sockfile = optarg;
            break;
        default:
            fprintf(stderr, "Invalid option\n");
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (debug) {
        fprintf(stderr, "[debug] port=%d conf=%s log=%s socket=%s\n", port,
                conffile ? conffile : "(none)",
                logfile ? logfile : "(none)",
                sockfile ? sockfile : "(none)");
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int one = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((unsigned short)port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) == -1) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Serveur demarre sur le port %d\n", port);
    fflush(stdout);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }
        handle_request(client_fd, debug);
        close(client_fd);
    }

    return EXIT_SUCCESS;
}