#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <signal.h>

#define MB 1024*1024

//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <sys/types.h>

uint64_t pcc_total[127];
uint64_t pcc_curr[127];
int stop = 0;
int connfd = -1;


/*initialize array with 0*/
void ini_arr(uint64_t *arr, int len)
{
    for (int i = 0; i < len; i++)
    {
        arr[i] = 0;
    }
}

/*count to pcc_curr the number of printable characters from buff*/
void update_pcc_cur(char *buff, int len)
{
    int index;
    for (int i = 0; i < len; i++)
    {
        if (buff[i] >= 32 && buff[i] <= 126)
        {
            index = buff[i]; /*will be the relevant number*/
            pcc_curr[index]++;
        }
    }
}

/*update pcc_total from pcc_curr*/
void update_pcc_total()
{
    for (int i = 0; i < 127; i++)
    {
        pcc_total[i] += pcc_curr[i];
    }
}

/*calculate C - number of printable characters*/
uint64_t calculate_C()
{
    uint64_t sum = 0;
    for (int i = 0; i < 127; i++)
    {
       sum += pcc_curr[i];
    }
    return sum;
}

/*print statistics end exit*/
void exit_server()
{
    for (int i = 32; i < 127; i++)
    {
        printf("char '%c' : %lu times\n", i, pcc_total[i]);
    }
    exit(0);
}

/*handle for SIGINT*/
void signal_handler(int signum)
{
    if (connfd < 0)
    {
        /*not connected right now, just exit*/
        exit_server();
    }

    /*else, raise flag so at the end of the connection, will exit*/
    stop = 1;
}


int main(int argc, char *argv[])
{
    uint16_t serv_port;
    uint64_t N, C;
    int listenfd = -1;
    int total_bytes_read = 0;
    int curr_bytes_read = 0;
    int bytes_not_read = 0;
    int num_bytes_for_curr_read = 0;
    int MB_bytes_not_read = 0;
    int total_bytes_sent = 0;
    int curr_bytes_sent = 0;
    int bytes_not_written = 0;
    char *chunk_of_file_buff = NULL;
    struct sigaction sig_act = {
        .sa_handler = signal_handler,
        .sa_flags = SA_RESTART /*handles EINTER case*/
    };


    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    memset(&serv_addr, 0, addrsize);

    if (argc != 2) /*1 arguments + path of program*/
    {
        fprintf(stderr, "1 argument is needed.\n");
        exit(1);
    }

    /*handle SIGINT*/
    if (sigaction(SIGINT, &sig_act, NULL) != 0)
    {
        perror("sigaction failed");
        exit(1);
    }

    serv_port = atoi(argv[1]);/
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htobe16(serv_port);
    serv_addr.sin_addr.s_addr = htobe32(INADDR_ANY);

    /*initialize the total pcc counter array*/
    ini_arr(pcc_total, 127);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("failed to create socket");
        exit(1);
    }
    if (bind(listenfd, (struct sockaddr*)&serv_addr, addrsize) != 0)
    {
        perror("bind failed");
        exit(1);
    }
    if (listen(listenfd, 10) != 0)
    {
        perror("listen failed");
        exit(1);
    }
    /*set SO_REUSEADDR - from https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr*/
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        perror("set SO_REUSEADDR failed");
    }

    start: /*for goto, when error occurs will return to here*/
    while (stop == 0)
    {
        connfd = accept(listenfd, NULL, &addrsize);
        if (connfd < 0)
        {
            perror("accept failed");
            exit(1);
        }

        total_bytes_read = 0;
        curr_bytes_read = 0;
        bytes_not_read = 0;
        
        /*receiving N - keep looping until nothing left to read*/
        bytes_not_read = sizeof(uint64_t);
        while (bytes_not_read > 0)
        {
            /*total_bytes_read = how much we've read so far
            curr_bytes_read = how much we've read in last read() call
            bytes_not_read =  how much we have left to read*/
            curr_bytes_read = read(connfd, &N + total_bytes_read, bytes_not_read);
            if (curr_bytes_read <= 0)
            {
                perror("reading N from client in server failed");
                if (errno != ETIMEDOUT && errno != ECONNRESET && errno != EPIPE)
                {
                    exit(1);
                }
                goto start;
            }
            total_bytes_read += curr_bytes_read;
            bytes_not_read -= curr_bytes_read;
        }
        N = be64toh(N);

        /*receiving file content - keep looping until nothing left to read*/
        total_bytes_read = 0;
        curr_bytes_read = 0;
        bytes_not_read = N;
        num_bytes_for_curr_read = 0;
        MB_bytes_not_read = 0;
        ini_arr(pcc_curr, 127);
        while (bytes_not_read > 0)
        {
            chunk_of_file_buff = calloc(MB, sizeof(char)); /*allocate 1MB*/
            num_bytes_for_curr_read = bytes_not_read > MB ? MB : bytes_not_read; /*if bytes_not_written is more than 1MB, we want to read now 1MB. else read all bytes left*/
            /*receiving num_bytes_for_curr_read - up to 1MB*/
            MB_bytes_not_read = num_bytes_for_curr_read;
            total_bytes_read = 0;
            while (MB_bytes_not_read > 0)
            {
                curr_bytes_read = read(connfd, chunk_of_file_buff + total_bytes_read, MB_bytes_not_read);
                if (curr_bytes_read <= 0)
                {
                    perror("reading file content from client in server failed");
                    if (errno != ETIMEDOUT && errno != ECONNRESET && errno != EPIPE)
                    {
                        exit(1);
                    }
                    goto start;
                }
                total_bytes_read += curr_bytes_read;
                MB_bytes_not_read -= curr_bytes_read;
            }
            /*update pcc_curr with current informatin*/
            update_pcc_cur(chunk_of_file_buff, num_bytes_for_curr_read);
            bytes_not_read -= num_bytes_for_curr_read;
        }
        free(chunk_of_file_buff);

        /*calculate C - number of printable characters*/
        C = calculate_C();
        /*sending C*/
        /*keep looping until nothing left to write*/
        C = htobe64(C);
        total_bytes_sent = 0;
        curr_bytes_sent = 0;
        bytes_not_written = sizeof(uint64_t);
        while (bytes_not_written > 0)
        {
            /*total_bytes_sent = how much we've written so far
            curr_bytes_sent = how much we've written in last write() call
            bytes_not_written =  how much we have left to write*/
            curr_bytes_sent = write(connfd, &C + total_bytes_sent, bytes_not_written);
            if (curr_bytes_sent <= 0)
            {
                perror("writng C from server to client failed");
                if (errno != ETIMEDOUT && errno != ECONNRESET && errno != EPIPE)
                {
                    exit(1);
                }
                goto start;
            }
            total_bytes_sent += curr_bytes_sent;
            bytes_not_written -= curr_bytes_sent;
        }

        /*update statistics*/
        update_pcc_total();        

        close(connfd);
        /*restart for new connection*/
        connfd = -1;
    }
    close(listenfd);
    exit_server();
}