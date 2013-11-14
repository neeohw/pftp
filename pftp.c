#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pgm/pgm.h>

void print_usage() {
    fprintf(stderr, "PFTP Client v0.0.1 by Maarten Vergouwe,\n"
            "Copyright Televic NV\n"
            "\n"
            "Usage : \n"
            "pftp [OPTIONS] file-to-send \n"
            "\tOPTIONS:\n"
            "\t\t-n multicast-address\t\tSelect multicast address on which file has to broadcasted to receivers.\n");
    exit(-1);
}

int main( int argc, char *argv[]) {
    int c = 0;
    int port = 0;
    char *network = NULL;
    pgm_error_t* pgm_err = NULL;
    pgm_sock_t *pgm_sock;

    while((c = getopt(argc, argv, "n:") != -1)) {
        switch (c) {
        case 'n':
            network = optarg;
            break;
        case '?':
            print_usage();
            break;
        default:
            print_usage();
            break;
        }
    }
    
    /* Initialize PGM */
    if (!pgm_init( &pgm_err )) {
        fprintf( stderr, "PGM init err: %s\n", pgm_err->message );
        pgm_error_free( pgm_err );
        return -1;
    }

    /* Get address information from IP addresss */
    struct pgm_addrinfo_t *pgm_addrinfo = NULL;
    if (!pgm_getaddrinfo( "239.192.3.1", NULL, &pgm_addrinfo, &pgm_err )) {
        fprintf( stderr, "Couldn't get address info: %s\n", pgm_err->message );
        pgm_error_free( pgm_err );
        return -1;
    }

    /* Create PGM socket */
    if (!pgm_socket( &pgm_sock, pgm_addrinfo->ai_send_addrs[0].gsr_group.ss_family,
                    SOCK_SEQPACKET, IPPROTO_PGM, &pgm_err)) {
        fprintf( stderr, "Create socket error: %s\n", pgm_err->message );
        pgm_error_free( pgm_err );
        return -1;
    }

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
            
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_SEND_ONLY, &send_only, sizeof(send_only));
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_MTU, &max_tpdu, sizeof(max_tpdu));
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_TXW_SQNS, &sqns, sizeof(sqns));
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_TXW_MAX_RTE, &max_rte, sizeof(max_rte));
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_AMBIENT_SPM, &ambient_spm, sizeof(ambient_spm));
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_HEARTBEAT_SPM, &heartbeat_spm, sizeof(heartbeat_spm));
  
    /* Create Global Session Identifier */
    struct pgm_sockaddr_t addr;
    memset (&addr, 0, sizeof(addr));
    addr.sa_port = port ? port : DEFAULT_DATA_DESTINATION_PORT;
    addr.sa_addr.sport = DEFAULT_DATA_SOURCE_PORT;
    if (!pgm_gsi_create_from_string (&addr.sa_addr.gsi, "pFTP GSI string 1", 0)) {
        fprintf (stderr, "Creating GSI: invalid parameters\n");
        return -1;
    }

    /* Bind socket to specified address */
    struct pgm_interface_req_t if_req;
    memset (&if_req, 0, sizeof(if_req));
    if_req.ir_interface = pgm_addrinfo->ai_recv_addrs[0].gsr_interface;
    if_req.ir_scope_id  = 0;
    if (AF_INET6 == pgm_addrinfo->ai_send_addrs[0].gsr_group.ss_family) {
        struct sockaddr_in6 sa6;
        memcpy (&sa6, &pgm_addrinfo->ai_recv_addrs[0].gsr_group, sizeof(sa6));
        if_req.ir_scope_id = sa6.sin6_scope_id;
    }
    if (!pgm_bind3 (pgm_sock,
            &addr, sizeof(addr),
            &if_req, sizeof(if_req),	/* tx interface */
            &if_req, sizeof(if_req),	/* rx interface */
            &pgm_err))
    {
        fprintf (stderr, "Binding PGM socket: %s\n", pgm_err->message);
        return -1;
    }

    const int blocking = 0,
          multicast_loop = 0,
          multicast_hops = 16,
          dscp = 0x2e << 2;		/* Expedited Forwarding PHB for network elements, no ECN. */

    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_MULTICAST_LOOP, &multicast_loop, sizeof(multicast_loop));
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_MULTICAST_HOPS, &multicast_hops, sizeof(multicast_hops));
    if (AF_INET6 != pgm_addrinfo->ai_send_addrs[0].gsr_group.ss_family) {
        pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_TOS, &dscp, sizeof(dscp));
    }
    pgm_setsockopt (pgm_sock, IPPROTO_PGM, PGM_NOBLOCK, &blocking, sizeof(blocking));

    if (!pgm_connect (pgm_sock, &pgm_err)) {
        fprintf (stderr, "Connecting PGM socket: %s\n", pgm_err->message);
        return -1;
    }

    return 0;
}
