#include "pftp_common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>

int pftp_create(const int sender, const char* network, const int port, pgm_sock_t **pgm_sock) {
    int ret_val = 0;
    char gsistr[20];
    gsistr[0] = '\0';

    pgm_error_t* pgm_err = NULL;
    sa_family_t sa_family = AF_UNSPEC;

    /* Initialize PGM */
    PRINT_DBG("PGM Initialization");
    if (!pgm_init( &pgm_err )) {
        PRINT_ERR("PGM init err: %s", pgm_err->message );
        ret_val = -1;
        goto ret_error;
    }

    /* Get address information from IP addresss */
    struct pgm_addrinfo_t *pgm_addrinfo = NULL;
    if (!pgm_getaddrinfo( network, NULL, &pgm_addrinfo, &pgm_err )) {
        PRINT_ERR("Couldn't get address info: %s", pgm_err->message );
        ret_val = -2;
        goto ret_error;
    }
    sa_family = pgm_addrinfo->ai_send_addrs[0].gsr_group.ss_family;

    /* Create PGM socket */
    PRINT_DBG("Creating PGM socket");
    if (!pgm_socket( pgm_sock, sa_family, SOCK_SEQPACKET, IPPROTO_PGM, &pgm_err)) {
        PRINT_ERR("Create socket error: %s", pgm_err->message );
        ret_val = -3;
        goto ret_error;
    }

    if (sender) {
        /* Set session parameters */
        const int send_only       = 1,
                  ambient_spm     = pgm_secs (30),
                  heartbeat_spm[] = { pgm_msecs (100),
                                      pgm_msecs (100),
                                      pgm_msecs (100),
                                      pgm_msecs (100),
                                      pgm_msecs (1300),
                                      pgm_secs  (7),
                                      pgm_secs  (16),
                                      pgm_secs  (25),
                                      pgm_secs  (30) },
                  max_tpdu        = 1500,
                  max_rte         = 400*1000,
                  sqns            = 100;

        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_SEND_ONLY, &send_only, sizeof(send_only));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_MTU, &max_tpdu, sizeof(max_tpdu));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_TXW_SQNS, &sqns, sizeof(sqns));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_TXW_MAX_RTE, &max_rte, sizeof(max_rte));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_AMBIENT_SPM, &ambient_spm, sizeof(ambient_spm));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_HEARTBEAT_SPM, &heartbeat_spm, sizeof(heartbeat_spm));

        strncat(gsistr, "pFTP Server GSI", 15);
    } else {
        /* set session parameters */
        const int recv_only = 1,
                passive = 0,
                peer_expiry = pgm_secs (300),
                spmr_expiry = pgm_msecs (250),
                nak_bo_ivl = pgm_msecs (50),
                nak_rpt_ivl = pgm_secs (2),
                nak_rdata_ivl = pgm_secs (2),
                nak_data_retries = 50,
                nak_ncf_retries = 50,
                max_tpdu        = 1500,
                sqns = 100;

        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_RECV_ONLY, &recv_only, sizeof(recv_only));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_PASSIVE, &passive, sizeof(passive));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_MTU, &max_tpdu, sizeof(max_tpdu));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_RXW_SQNS, &sqns, sizeof(sqns));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_PEER_EXPIRY, &peer_expiry, sizeof(peer_expiry));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_SPMR_EXPIRY, &spmr_expiry, sizeof(spmr_expiry));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_NAK_BO_IVL, &nak_bo_ivl, sizeof(nak_bo_ivl));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_NAK_RPT_IVL, &nak_rpt_ivl, sizeof(nak_rpt_ivl));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_NAK_RDATA_IVL, &nak_rdata_ivl, sizeof(nak_rdata_ivl));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_NAK_DATA_RETRIES, &nak_data_retries, sizeof(nak_data_retries));
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_NAK_NCF_RETRIES, &nak_ncf_retries, sizeof(nak_ncf_retries));

        strncat(gsistr, "pFTP Client GSI", 15);
    }

    /* Create Global Session Identifier */
    PRINT_DBG("Creating PGM GSI");
    struct pgm_sockaddr_t addr;
    memset (&addr, 0, sizeof(addr));
    addr.sa_port = port ? port : DEFAULT_DATA_DESTINATION_PORT;
    addr.sa_addr.sport = DEFAULT_DATA_SOURCE_PORT;
    if (!pgm_gsi_create_from_string (&addr.sa_addr.gsi, gsistr, strlen(gsistr))) {
        PRINT_ERR("Creating GSI: invalid parameters");
        ret_val = -4;
        goto ret_error;
    }

    /* join IP multicast groups */
    unsigned int i=0;
    for (; i < pgm_addrinfo->ai_recv_addrs_len; i++)
        pgm_setsockopt(*pgm_sock, IPPROTO_PGM, PGM_JOIN_GROUP, &pgm_addrinfo->ai_recv_addrs[i], sizeof(struct group_req));
    pgm_setsockopt(*pgm_sock, IPPROTO_PGM, PGM_SEND_GROUP, &pgm_addrinfo->ai_send_addrs[0], sizeof(struct group_req));

    /* Bind socket to specified address */
    struct pgm_interface_req_t if_req;
    memset (&if_req, 0, sizeof(if_req));
    if_req.ir_interface = pgm_addrinfo->ai_recv_addrs[0].gsr_interface;
    if_req.ir_scope_id  = 0;
    if (AF_INET6 == sa_family) {
        struct sockaddr_in6 sa6;
        memcpy (&sa6, &pgm_addrinfo->ai_recv_addrs[0].gsr_group, sizeof(sa6));
        if_req.ir_scope_id = sa6.sin6_scope_id;
    }
    PRINT_DBG("Bind socket");
    if (!pgm_bind3 (*pgm_sock,
            &addr, sizeof(addr),
            &if_req, sizeof(if_req),	/* tx interface */
            &if_req, sizeof(if_req),	/* rx interface */
            &pgm_err))
    {
        PRINT_ERR("Binding PGM socket: %s", pgm_err->message);
        ret_val = -5;
        goto ret_error;
    }

    const int nonblocking = 0,
          multicast_loop = 1,
          multicast_hops = 16,
          dscp = 0x2e << 2;		/* Expedited Forwarding PHB for naddr elements, no ECN. */

    pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_MULTICAST_LOOP, &multicast_loop, sizeof(multicast_loop));
    pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_MULTICAST_HOPS, &multicast_hops, sizeof(multicast_hops));
    if (AF_INET6 != sa_family) {
        pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_TOS, &dscp, sizeof(dscp));
    }
    pgm_setsockopt (*pgm_sock, IPPROTO_PGM, PGM_NOBLOCK, &nonblocking, sizeof(nonblocking));

    PRINT_DBG("Connecting socket");
    if (!pgm_connect (*pgm_sock, &pgm_err)) {
        PRINT_ERR("Connecting PGM socket: %s", pgm_err->message);
        ret_val = -6;
        goto ret_error;
    }

    goto ret_good;

ret_error:
    PRINT_DBG("Return error");
    if (NULL != pgm_addrinfo) {
        pgm_freeaddrinfo(pgm_addrinfo);
        pgm_addrinfo = NULL;
    }
    if (NULL != pgm_err) {
        pgm_freeaddrinfo(pgm_addrinfo);
        pgm_addrinfo = NULL;
    }
    if (NULL != pgm_sock) {
        pgm_close(*pgm_sock, FALSE);
        pgm_sock = NULL;
    }
    goto ret;
ret_good:
    PRINT_DBG("Return good");
ret:
    return ret_val;
}

int pftp_stop(pgm_sock_t *pgm_sock) {
    if (NULL != pgm_sock) {
        pgm_close(pgm_sock, FALSE);
        pgm_sock = NULL;
    }
}

/* Don't forget to free the return value! */
char *pftp_inet_iftoa(char *iface) {
    /* Check if we can resolve interface name to IP address (bug in PGM?) */
    if (iface) {
        char *network = (char*)malloc(128);
        network[0] = '\0';

        struct ifreq ifr;
        int ret;

        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

        ifr.ifr_addr.sa_family = AF_INET;

        strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);

        ret = ioctl(sockfd, SIOCGIFADDR, &ifr);
        close(sockfd);

        if (ret) {
            PRINT_ERR("Couldn't resolve interface to IP address!");
            free(network);
            return NULL;
        }

        strncpy(network, inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr), 128);
        PRINT_DBG("IP address from interface: %s", network);

        return network;
    }

    return NULL;
}

int pftp_parse_cmd(char* buf, void **cmd_info) {
    if (0 == strncmp(buf, CMD_SEND_FILE, 2)) {
        PRINT_DBG("Received command: %s", buf);

        char *str_size = strchr(buf, ' ');
        if (!str_size) {
            goto wrong_cmd;
        }

        struct pftp_send_file_cmd *sf_cmd = malloc(sizeof(*sf_cmd));
        char *str_name;

        sf_cmd->fsize = (unsigned int)strtol((str_size+1), &str_name, 0);
        if (0 == sf_cmd->fsize || !str_name) {
            free(sf_cmd);
            goto wrong_cmd;
        }

        strncpy(sf_cmd->fname, (str_name+1), FNAMSIZE);

        *cmd_info = (void*)sf_cmd;
        return PFTP_CMD_SEND_FILE_PARSED;
    }

    return PFTP_CMD_UNKNOWN;
wrong_cmd:
    PRINT_ERR("Wrong command received!");
    return PFTP_CMD_UNKNOWN;
}

void pftp_free_cmd(void *cmd_info) {
    if (cmd_info) {
        free(cmd_info);
        cmd_info = NULL;
    }
}
