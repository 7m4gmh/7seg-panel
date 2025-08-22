#include "udp.h"
#include "common.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main() {
    int i2c_fd=open("/dev/i2c-0", O_RDWR);
    if(i2c_fd < 0) {
        perror("open /dev/i2c-0");
        return 1;
    }

    int sockfd=socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr{};
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=INADDR_ANY;
    servaddr.sin_port=htons(PORT);

    if(bind(sockfd,(sockaddr*)&servaddr,sizeof(servaddr))<0){
        perror("bind");
        return 1;
    }

    udp_loop(sockfd,i2c_fd);

    close(sockfd);
    close(i2c_fd);
    return 0;
}
