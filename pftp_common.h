#include <pgm/pgm.h>
#include <stdio.h>

//#define UDPTEST

#define PRINT_DBG(x, ...)       printf(x"\n", ##__VA_ARGS__)
#define PRINT_ERR(x, ...)       fprintf(stderr, x"\n", ##__VA_ARGS__)

#define PFTP_DEFAULT_MCAST_ADDR "239.192.0.1"

#define PGMBUF_SIZE             (1436)
#define FNAMSIZE                (128)

#define PFTP_UDP_PORT           (5698)

#define CMD_SEND_FILE       "sf"

enum {
    PFTP_CMD_UNKNOWN = -1,
    PFTP_CMD_SEND_FILE_PARSED = 0
} ParseCommands_t;

struct pftp_send_file_cmd {
    unsigned int fsize;
    char fname[FNAMSIZE];
};

int     pftp_create(const int sender, const char *network, const int port, pgm_sock_t **pgm_sock);
int     pftp_stop();
char    *pftp_inet_iftoa(char *iface);
int     pftp_parse_cmd(char* buf, void **cmd_info);
void    pftp_free_cmd(void* cmd_info);
