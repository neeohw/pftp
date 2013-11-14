#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pgm/pgm.h>

void print_usage() {
    fprintf(stderr, "PFTP Client v0.0.1 by Maarten Vergouwe,\n
            Copyright Televic NV\n
            \n
            Usage : \n
            pftp [OPTIONS] file-to-send \n
            \tOPTIONS:\n
            \t\t-n multicast-address\t\tSelect multicast address on which file has to broadcasted to receivers.\n
            ");
    exit(-1);
}

int main( int argc, char *argv[]) {
    int c;
    char *network = NULL;
    pgm_error_t* pgm_err = NULL;
    pgm_sock_t *pgm_sock;
    struct pgm_addrinfo_t *pgm_addrinfo = NULL;

    while((c = getopt(argc, argv, "n:") != -1) {
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
    
    if (!pgm_init( &pgm_err )) {
        fprintf( stderr, "PGM init err: %s\n", pgm_err->message );
        pgm_error_free( pgm_err );
        return -1;
    }

    if (!pgm_getaddrinfo( "239.192.3.1", NULL, &pgm_addrinfo, &pgm_err )) {
        fprintf( stderr, "Couldn't get address info: %s\n", pgm_err->message );
        pgm_error_free( pgm_err );
        return -1;
    }

    if (!pgm_socket( &pgm_sock, pgm_addrinfo->ao_send_addrs[0].gsr_group.ss_family,
                    SOCK_SEQPACKET, IPPROTO_PGM, &pgm_err)) {
        fprintf( stderr, "Create socket error: %s\n", pgm_err->message );
        pgm_error_free( pgm_err );
        return -1;
    }

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
            
    pgm_setsockopt (sock, IPPROTO_PGM, PGM_SEND_ONLY, &send_only, sizeof(send_only));
    pgm_setsockopt (sock, IPPROTO_PGM, PGM_MTU, &max_tpdu, sizeof(max_tpdu));
    pgm_setsockopt (sock, IPPROTO_PGM, PGM_TXW_SQNS, &sqns, sizeof(sqns));
    pgm_setsockopt (sock, IPPROTO_PGM, PGM_TXW_MAX_RTE, &max_rte, sizeof(max_rte));
    pgm_setsockopt (sock, IPPROTO_PGM, PGM_AMBIENT_SPM, &ambient_spm, sizeof(ambient_spm));
    pgm_setsockopt (sock, IPPROTO_PGM, PGM_HEARTBEAT_SPM, &heartbeat_spm, sizeof(heartbeat_spm));eturn -1;
  
    struct pgm_sockaddr_t addr;
    memset (&addr, 0, sizeof(addr));
    addr.sa_port = port ? port : DEFAULT_DATA_DESTINATION_PORT;
    addr.sa_addr.sport = DEFAULT_DATA_SOURCE_PORT;
    if (!pgm_gsi_create_from_hostname (&addr.sa_addr.gsi, &pgm_err)) {
        fprintf (stderr, "Creating GSI: %s\n", pgm_err->message);
        goto err_abort;
    }

    return 0;
}
