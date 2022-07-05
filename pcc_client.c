#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>

#define MB 1024*1024

//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <sys/types.h>

int main(int argc, char *argv[])
{
    uint16_t serv_port;
    char *path_file = NULL;
    FILE *fp = NULL;
    int fd = -1;
    uint64_t N, C;
    int sockfd = -1;
    int total_bytes_sent = 0;
    int curr_bytes_sent = 0;
    int bytes_not_written = 0;
    int curr_bytes_read_to_buff = 0;
    int MB_bytes_not_written = 0;
    int num_bytes_for_curr_read = 0;
    int total_bytes_read = 0;
    int curr_bytes_read = 0;
    int bytes_not_read = 0;
    char *chunk_of_file_buff = NULL;
    
    
    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, addrsize);

    if (argc != 4) /*3 arguments + path of program*/
    {
        fprintf(stderr, "3 arguments are needed.\n");
        exit(1);
    }

    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
    serv_port = atoi(argv[2]);
    path_file = argv[3];

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htobe16(serv_port);
    
    /*open for finding N*/
    fp = fopen(path_file, "r");
    if (fp == NULL)
    {
        perror("open file has failed");
        exit(1);
    }
    /*find N - from https://www.geeksforgeeks.org/c-program-find-size-file/*/
    fseek(fp, 0L, SEEK_END);
    N = ftell(fp);
    fclose(fp);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("failed to create socket");
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr*)&serv_addr, addrsize) < 0)
    {
        perror("failed to connect");
        exit(1);
    }
    
    /*sending N*/
    /*keep looping until nothing left to write*/
    N = htobe64(N);
    bytes_not_written = sizeof(uint64_t);
    while (bytes_not_written > 0)
    {
        /*total_bytes_sent = how much we've written so far
        curr_bytes_sent = how much we've written in last write() call
        bytes_not_written =  how much we have left to write*/
        curr_bytes_sent = write(sockfd, &N + total_bytes_sent, bytes_not_written);
        if (curr_bytes_sent < 0)
        {
            perror("writng N from client to server failed");
            exit(1);
        }
        total_bytes_sent += curr_bytes_sent;
        bytes_not_written -= curr_bytes_sent;
    }
    N = be64toh(N);

    curr_bytes_sent = 0;
    bytes_not_written = N;
    /*open for reading file*/
    fd = open(path_file, O_RDONLY);
    if (fd < 0)
    {
        perror("open file has failed");
        exit(1);
    }
    curr_bytes_read_to_buff = 0;
    MB_bytes_not_written = 0;
    num_bytes_for_curr_read = 0;
    /*sending file content, read from file then send to server*/
    while (bytes_not_written > 0)
    {
        chunk_of_file_buff = calloc(MB, sizeof(char)); /*allocate 1MB*/
        num_bytes_for_curr_read = bytes_not_written > MB ? MB : bytes_not_written; /*if bytes_not_written is more than 1MB, we want to read now 1MB. else read all bytes left*/
        curr_bytes_read_to_buff = read(fd, chunk_of_file_buff, num_bytes_for_curr_read);
        if (curr_bytes_read_to_buff < 0)
        {
            perror("reading from file in client side failed");
            exit(1);
        }

        /*send curr_bytes_read_to_buff - up to 1MB*/
        MB_bytes_not_written = curr_bytes_read_to_buff;
        total_bytes_sent = 0;
        while (MB_bytes_not_written > 0)
        {
            curr_bytes_sent = write(sockfd, chunk_of_file_buff + total_bytes_sent, MB_bytes_not_written);
            if (curr_bytes_sent < 0)
            {
                perror("writng file content from client to server failed");
                exit(1);
            }
            total_bytes_sent += curr_bytes_sent;
            MB_bytes_not_written -= curr_bytes_sent;
        }
        bytes_not_written -= curr_bytes_read_to_buff;
    }
    close(fd);
    free(chunk_of_file_buff);

    /*receiving C - keep looping until nothing left to read*/
    total_bytes_read = 0;
    curr_bytes_read = 0;
    bytes_not_read = sizeof(uint64_t);
    while (bytes_not_read > 0)
    {
        /*total_bytes_read = how much we've read so far
        curr_bytes_read = how much we've read in last read() call
        bytes_not_read =  how much we have left to read*/
        curr_bytes_read = read(sockfd, &C + total_bytes_read, bytes_not_read);
        if (curr_bytes_read < 0)
        {
            perror("reading C from server in client failed");
            exit(1);
        }
        total_bytes_read += curr_bytes_read;
        bytes_not_read -= curr_bytes_read;
    }
    C = be64toh(C);
    printf("# of printable characters: %lu\n", C);
    close(sockfd);
    exit(0);
}