#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "pftp_common.h"

typedef enum {
    STATE_WAIT_CMD = 0,
    STATE_FILE_TRANSFER,
} PFTPStates_t;

static int      m_run_receive_loop = 0;

static void print_usage() {
    fprintf(stderr, "PFTP Client v0.0.1 by Maarten Vergouwe,\n"
            "Copyright Televic NV\n"
            "\n"
            "Usage : \n"
            "pftpd [OPTIONS]\n"
            "\n"
            "OPTIONS:\n"
            "\t-m multicast-address\tSelect multicast address to receive on.\n"
            "\t-i interface\t\tSelect interface to receive on.\n"
            "\t-p receive port\t\tSelect port to receive on.\n");
    exit(-1);
}

static void handle_sigint(int signo) {
    m_run_receive_loop = 0;
}

int main( int argc, char *argv[]) {
    int c = 0;
    int port = 0;
    int run_state = STATE_WAIT_CMD;
    char network[128];
    char *iface = NULL;
    char *mcast_addr = NULL;

    network[0] = '\0';

    pgm_error_t* pgm_err = NULL;
    pgm_sock_t *pgm_sock = NULL;

    signal(SIGINT, handle_sigint);

    while((c = getopt(argc, argv, "hm:i:p:")) != -1) {
        switch (c) {
        case 'm':
            mcast_addr = optarg;
            break;
        case 'i':
            iface = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'h':
        case '?':
            print_usage();
            break;
        default:
            print_usage();
            break;
        }
    }

#ifdef UDPTEST
    int sockfd = 0;
    struct sockaddr_in servaddr;
    struct ip_mreqn mreq;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = port ? htons(port) : htons(PFTP_UDP_PORT);

    if (0 != bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ) {
        PRINT_ERR("Couldn't bind socket!");
        return -1;
    }

    memset(&mreq, 0, sizeof(mreq));
    char *ifaddr = NULL;
    if (iface) {
        ifaddr = pftp_inet_iftoa(iface);
    } else {
        ifaddr = pftp_inet_iftoa("eth0");
    }

    mreq.imr_address.s_addr = inet_addr(ifaddr);
    mreq.imr_ifindex = 0;
    if (ifaddr) free(ifaddr);

    if (mcast_addr) {
        mreq.imr_multiaddr.s_addr = inet_addr(mcast_addr);
    } else {
        mreq.imr_multiaddr.s_addr = inet_addr(PFTP_DEFAULT_MCAST_ADDR);
    }

    if (0 != setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
        PRINT_ERR("Couldn't join multicast group!: %s" ,strerror(errno));
    }

#else
    if (iface) {
        char *ifaddr = pftp_inet_iftoa(iface);
        if (strlen(ifaddr) > 0) {
            strncat(network, ifaddr, 128);
            strncat(network, ";", 128);
        } else {
            free(ifaddr);
            return -1;
        }
        free(ifaddr);
    }

    if (mcast_addr) {
        strncat(network, mcast_addr, 128);
    } else {
        strncat(network, PFTP_DEFAULT_MCAST_ADDR, 128);
    }

    if (0 != pftp_create(0, network, port, &pgm_sock)) {
        return -1;
    }
#endif

    PRINT_DBG("Running receive loop");
    int fds = 0;
    fd_set readfds;
    int file_desc = -1;
    int total_bytes = 0;
    void *cmd_info = NULL;

    m_run_receive_loop = 1;

    while (m_run_receive_loop) {

        char recv_buf[PGMBUF_SIZE];
        memset(&recv_buf, 0, PGMBUF_SIZE);
        size_t bytes_read = 0;

#ifdef UDPTEST
        fds = sockfd + 1;

        bytes_read = recv(sockfd, recv_buf, PGMBUF_SIZE, MSG_DONTWAIT);

        switch (bytes_read) {
        case -1:
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                FD_ZERO(&readfds);
                FD_SET(sockfd, &readfds);
                select(fds, &readfds, NULL, NULL, NULL);
            }
            break;
        default:
        {
#else
        struct timeval tv;

        struct pgm_sockaddr_t from;
        socklen_t from_sz = sizeof(from);
        const int pgm_status = pgm_recvfrom(pgm_sock, recv_buf, PGMBUF_SIZE, MSG_DONTWAIT, &bytes_read, &from, &from_sz, &pgm_err);

        //PRINT_DBG("pgm_status: %d", pgm_status);
        switch (pgm_status) {

        case PGM_IO_STATUS_NORMAL :
        {
#endif
            int cmd_parsed = pftp_parse_cmd(recv_buf, &cmd_info);

            switch(cmd_parsed) {

            case PFTP_CMD_UNKNOWN :
                if (  run_state == STATE_FILE_TRANSFER
                   && file_desc > 0 ) {
                    write(file_desc, recv_buf, bytes_read);
                    total_bytes += bytes_read;
                    if (total_bytes >= ((struct pftp_send_file_cmd*)cmd_info)->fsize) {
                        PRINT_DBG("File %s received.", ((struct pftp_send_file_cmd*)cmd_info)->fname);
                        close(file_desc);
                        total_bytes = 0;
                        pftp_free_cmd(cmd_info);
                        run_state = STATE_WAIT_CMD;
                    }
                }
                break;

            case PFTP_CMD_SEND_FILE_PARSED :
            {
                struct pftp_send_file_cmd *sf_cmd = (struct pftp_send_file_cmd *)cmd_info;
                file_desc = open(sf_cmd->fname, O_CREAT | O_WRONLY, S_IRWXU);
                if (file_desc < 0) {
                    PRINT_ERR("Couldn't open file for writing! %s", strerror(errno));
                    pftp_free_cmd(cmd_info);
                    run_state = STATE_WAIT_CMD;
                } else {
                    run_state = STATE_FILE_TRANSFER;
                }

            } break;

            default :
                pftp_free_cmd(cmd_info);
                break;
            }

#ifdef UDPTEST
        } break;
        }
#else
        } break;
        case PGM_IO_STATUS_TIMER_PENDING:
        {
            socklen_t optlen = sizeof(tv);
            pgm_getsockopt (pgm_sock, IPPROTO_PGM, PGM_TIME_REMAIN, &tv, &optlen);
            if (0 == (tv.tv_sec * 1000) + ((tv.tv_usec + 500) / 1000))
                break;
            goto block;
        }
        case PGM_IO_STATUS_RATE_LIMITED:
        {
            socklen_t optlen = sizeof(tv);
            pgm_getsockopt (pgm_sock, IPPROTO_PGM, PGM_RATE_REMAIN, &tv, &optlen);
            if (0 == (tv.tv_sec * 1000) + ((tv.tv_usec + 500) / 1000))
                break;
            /* No accidental fallthrough! */
        }
        case PGM_IO_STATUS_WOULD_BLOCK:
block:
            FD_ZERO(&readfds);
            pgm_select_info(pgm_sock, &readfds, NULL, &fds);
            fds = select(fds, &readfds, NULL, NULL, pgm_status == PGM_IO_STATUS_WOULD_BLOCK ? NULL : &tv);
            break;
        case PGM_IO_STATUS_RESET:
            if (pgm_err) {
                fprintf(stderr, "%s\n", pgm_err->message);
                pgm_error_free(pgm_err);
                pgm_err = NULL;
            }
            close(file_desc);
            total_bytes = 0;
            run_state = STATE_WAIT_CMD;
            break;
        default :
            if (pgm_err) {
                fprintf(stderr, "%s\n", pgm_err->message);
                pgm_error_free(pgm_err);
                pgm_err = NULL;
            }
            if (pgm_status == PGM_IO_STATUS_ERROR) {
                m_run_receive_loop = 0;
            } else {
                PRINT_ERR("Unknown status!");
                m_run_receive_loop = 0;
            }
            break;
        }
#endif
    }

    if (file_desc > 0)
        close(file_desc);
    PRINT_DBG("Receive loop finished");

#ifdef UDPTEST
    if (sockfd > 0)
        close(sockfd);
#else
    if (pgm_sock) {
        pftp_stop(pgm_sock);
    }
#endif

    return 0;
}
