#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <oping.h>
#include <errno.h>

//sleep time for loop
#define SLEEP_TIME 200000

//timeout for ping attempt
#define PING_TIMEOUT 1.0

//timeout for tcp connect
#define SOCKET_TIMEOUT 2000

//timeout for getaddrinfo
#define RESOLV_TIMEOUT 3

//time after a host is beeing viewed as unreachable (inf = 0)
#define WAIT_TIMEOUT 0


enum { UNREACH_EC=1, INT_EC, OTHER_EC };

#define HELP(X) helpmsg(argv[0],X)
#define DBG(X) if(debug) fputs("[debug] "X"\n",stderr)


// holds parsed config
struct config {
    unsigned long sleep_time;
    double ping_timeout;
    unsigned long socket_timeout;
    unsigned long wait_timeout;
    unsigned long resolv_timeout;
    int8_t infinite_resolv;
} config;

//holds host information
struct host {
    char *raw_host;
    char *raw_port;
    struct addrinfo *res;
} host;

//needed for cleanup
int socket_d=-1;
pingobj_t *ping_inst=NULL;

//needed for debugging
int8_t debug=0;

//cleans up
void interrupt_handler(int e) {
    if(socket_d != -1)
        close(socket_d);
    if(ping_inst != NULL)
        ping_destroy(ping_inst);
    exit(INT_EC);
}

void helpmsg(char *pname,char *s) {
    if(s != NULL)
        fprintf(stderr,"%s\n",s);
    printf("Usage: %s <options> [host] [?port]\n"
           "\t -h: display this help message\n"
           "\t -d: enable debug output\n"
           "\t -r[secs]: resolv timeout\n"
           "\t -R: infinite resolv\n"
           "\t -t[secs]: set timeout in seconds\n"
           "\t -s[msecs]: set sleep time between connection/ping attempts\n"
           "\t -1[secs]: set ping timeout\n"
           "\t -2[msecs]: set socket timeout\n",pname);
}


int8_t connectloop(void) {
    DBG("in connectloop");
    int socket_d;
    int8_t ret_val=0;
    struct timeval timeout;
    timeout.tv_sec  = config.socket_timeout/1000;
    timeout.tv_usec = config.socket_timeout%1000;
    socket_d=socket(host.res->ai_family,SOCK_STREAM,0);
    if(socket_d == -1) {
        fprintf(stderr,"Error: could not create socket!\n");
        return OTHER_EC;
    }
    setsockopt(socket_d, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));


    if(config.wait_timeout != 0) {
        DBG("now we wait with timeout");

        time_t start_time=time(NULL);
        while(connect(socket_d,host.res->ai_addr,host.res->ai_addrlen)<0) {
            if(time(NULL) - start_time > config.wait_timeout) {
                ret_val=UNREACH_EC;
                break;
            }
            usleep(config.sleep_time);
        }
    } else {
        DBG("now we wait without timeout");
        while(connect(socket_d,host.res->ai_addr,host.res->ai_addrlen)<0)
            usleep(config.sleep_time);
    }
    close(socket_d);
    return ret_val;
}


int8_t pingloop(void) {
    DBG("in pingloop");
    int8_t ret_val=0;

    ping_inst= ping_construct();

    if(ping_inst == NULL) {
        fprintf(stderr,"Error: could not instanciate ping!\n");
        ping_destroy(ping_inst);
        return OTHER_EC;
    }
    if(ping_setopt(ping_inst,PING_OPT_TIMEOUT,&config.ping_timeout) != 0) {
        fprintf(stderr,"%s\nError: could not instanciate ping timeout!\n",ping_get_error(ping_inst));
        ping_destroy(ping_inst);
        return OTHER_EC;
    }
    if(ping_setopt(ping_inst,PING_OPT_AF,&(host.res->ai_family)) != 0) {
        fprintf(stderr,"%s\nError: could not set af type!\n",ping_get_error(ping_inst));
        ping_destroy(ping_inst);
        return OTHER_EC;
    }
    if(ping_host_add(ping_inst,host.raw_host) != 0) {
        fprintf(stderr,"%s\nError: could not instanciate host to ping!\n",ping_get_error(ping_inst));
        ping_destroy(ping_inst);
        return OTHER_EC;
    }

    if(config.wait_timeout != 0) {
        DBG("now we wait with timeout");
        time_t start_time=time(NULL);
        while(1) {
            register int e= ping_send(ping_inst);
            if(e >=1) //host is up
                break;
            else if(time(NULL) -start_time > config.wait_timeout) { // timeout arrived
                ret_val=UNREACH_EC;
                break;
            } else if(e < 0) {
                printf("Error: %s\n",ping_get_error(ping_inst));
                ret_val=OTHER_EC;
                break;
            }
            usleep(config.sleep_time);
        }
    } else {
        DBG("now we wait without timeout");
        while(1) {
            register int e= ping_send(ping_inst);
            if(e >=1)// host is up
                break;
            else if(e < 0) {
                printf("Error: %s\n",ping_get_error(ping_inst));
                ret_val=OTHER_EC;
                break;
            }
            usleep(config.sleep_time);
        }
    }
    ping_destroy(ping_inst);
    return ret_val;
}

int8_t set_ip_addr(void) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    time_t start_time=time(NULL);
    while(config.infinite_resolv || time(NULL)-start_time < config.resolv_timeout) {
        if(getaddrinfo(host.raw_host,host.raw_port,&hints,&(host.res)) == 0 && host.res != NULL)
            return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if(argc == 1) {
        HELP(NULL);
        return OTHER_EC;
    }

    signal(SIGINT,interrupt_handler);
    signal(SIGTERM,interrupt_handler);

    int8_t exit_code=0;
    config.sleep_time=SLEEP_TIME;
    config.ping_timeout=PING_TIMEOUT;
    config.wait_timeout=WAIT_TIMEOUT;
    config.socket_timeout=SOCKET_TIMEOUT;
    config.resolv_timeout=RESOLV_TIMEOUT;
    config.infinite_resolv=0;
    host.res=NULL;
    register int op;

    while((op=getopt(argc,argv,"hds:1:2:t:r:R")) != -1) {
        switch(op) {
        case 'h':
            HELP(NULL);
            return OTHER_EC;
            break;
        case 'd':
            debug=1;
            break;
        case 's':
            if(sscanf(optarg,"%lu",&(config.sleep_time)) != 1) {
                HELP("Error: -s: invalid argument!");
                exit_code=OTHER_EC;
            }
            break;
        case '1':
            if(sscanf(optarg,"%lf",&(config.ping_timeout)) != 1) {
                HELP("Error: -1: invalid argument!");
                exit_code=OTHER_EC;
            }
            break;
        case '2':
            if(sscanf(optarg,"%lu",&(config.socket_timeout))!= 1) {
                HELP("Error: -2: invalid argument!");
                exit_code=OTHER_EC;
            }
            break;

        case 't':
            if(sscanf(optarg,"%lu",&(config.wait_timeout)) != 1) {
                HELP("Error: -t: invalid argument!");
                exit_code=OTHER_EC;
            }
            break;
        case 'r':
            if(sscanf(optarg,"%lu",&(config.resolv_timeout)) != 1) {
                HELP("Error: -r: invalid argument!");
                exit_code=OTHER_EC;
            }
            break;
        case 'R':
            config.infinite_resolv=1;
            break;
        case '?':
            exit_code=OTHER_EC;
            break;
        default:
            exit_code=OTHER_EC;
            HELP(NULL);
        }
        if(exit_code)
            break;
    }

    if(debug)
        fprintf(stderr,"[debug] parsed:\n\t sleep_timeout=%lu ping_timeout=%lf socket_timeout=%lu wait_timeout=%lu\n",
                config.sleep_time, config.ping_timeout,config.socket_timeout,config.wait_timeout);

    if(optind > argc-1) {
        fputs("Error: need host\n",stderr);
        exit_code=OTHER_EC;
    } else if(optind == argc-2) {
        host.raw_port=argv[optind+1];
        host.raw_host=argv[optind];
    } else if(optind == argc-1)
        host.raw_host=argv[optind];
    else {
        fputs("Error: invalid operand range\n",stderr);
        exit_code=OTHER_EC;
    }

    if(exit_code != 0)
        return exit_code;

    if(!set_ip_addr()) {
        fputs("Error: erroneous host (or port)\n",stderr);
        exit_code=OTHER_EC;
    }

    if(exit_code == 0) {
        if(host.raw_port==NULL)
            exit_code=pingloop();
        else
            exit_code= connectloop();
    }

    freeaddrinfo(host.res);
    return exit_code;
}
