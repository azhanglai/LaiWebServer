#ifndef _SOCKET_H
#define _SOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

int Socket(const char* host, int Port) {
    struct sockaddr_in cli_addr;
    struct hostent *hp;

    memset(&cli_addr, 0, sizeof(cli_addr));

    cli_addr.sin_family = AF_INET;
    unsigned long Ip = inet_addr(host);

    if (Ip != INADDR_NONE) {
        memcpy(&cli_addr.sin_addr, &Ip, sizeof(Ip));
    } else {
        hp = gethostbyname(host);
        if (hp == NULL) {
            return -1;
        }
        memcpy(&cli_addr.sin_addr, hp->h_addr, hp->h_length);
    }
    cli_addr.sin_port = htons(Port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return sockfd;
    }

    if (connect(sockfd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

#endif 

