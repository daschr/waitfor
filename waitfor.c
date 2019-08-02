#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <oping.h>
#include <errno.h>
extern int h_errno;

#define HELP(X) helpmsg(argv[0],X)
#define DBG(X) if(debug) fputs("[debug] "X"\n",stderr)
struct config{
	unsigned long sleep_time;
	double ping_timeout;
	unsigned long socket_timeout;
	unsigned long wait_timeout;
}config;

struct host{
	char *raw_host;
	char *raw_port;
	struct addrinfo *res;
} host;

int8_t debug=0;

void helpmsg(char *pname,char *s){
	if(s != NULL)
		fprintf(stderr,"%s\n",s);
	printf("Usage: %s <options> [host] [?port]\n\
\t -h: display this help message\n\
\t -d: enable debug output\n\
\t -t[secs]: set timeout in seconds\n\
\t -s[msecs]: set sleep time between connection/ping attempts\n\
\t -1[secs]: set ping timeout\n\
\t -2[msecs]: set socket timeout\n",pname);
}


int8_t connectloop(void){
	DBG("in connectloop");
	int socket_d;
	int8_t ret_val=0;
	struct timeval timeout;
	timeout.tv_sec  = config.socket_timeout/1000; 
	timeout.tv_usec = config.socket_timeout%1000;
	socket_d=socket(host.res->ai_family,SOCK_STREAM,0);
	if(socket_d == -1){
		fprintf(stderr,"Error: could not create socket!\n");
		return 1;
	}
	setsockopt(socket_d, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));


	if(config.wait_timeout != 0){
		DBG("now we wait with timeout");
		
		time_t start_time=time(NULL);
		while(connect(socket_d,host.res->ai_addr,host.res->ai_addrlen)<0){
			if(time(NULL) - start_time > config.wait_timeout){ 
				ret_val=2;
				break;
			}
			usleep(config.sleep_time);
		}
	}else{
		DBG("now we wait without timeout");
		while(connect(socket_d,host.res->ai_addr,host.res->ai_addrlen)<0)
			usleep(config.sleep_time);
	}	
	close(socket_d);
	return ret_val;
} 


int8_t pingloop(void){
	DBG("in pingloop");
	int8_t ret_val=0;

	pingobj_t *ping_inst= ping_construct();
	
	if(ping_inst == NULL){
		fprintf(stderr,"Error: could not instanciate ping!\n");
		free(ping_inst);
		return 1;
	}
	if(ping_setopt(ping_inst,PING_OPT_TIMEOUT,&config.ping_timeout) != 0){
		fprintf(stderr,"%s\nError: could not instanciate ping timeout!\n",ping_get_error(ping_inst));
		free(ping_inst);
		return 1;
	}
	if(ping_setopt(ping_inst,PING_OPT_AF,&(host.res->ai_family)) != 0){
		fprintf(stderr,"%s\nError: could not set af type!\n",ping_get_error(ping_inst));
		free(ping_inst);
		return 1;
	}
	if(ping_host_add(ping_inst,host.raw_host) != 0){
		fprintf(stderr,"%s\nError: could not instanciate host to ping!\n",ping_get_error(ping_inst));
		free(ping_inst);
		return 1;
	}

	if(config.wait_timeout != 0){
		DBG("now we wait with timeout");
		time_t start_time=time(NULL);
		while(1){
			register int e= ping_send(ping_inst);
			if(e >=1) //host is up
				break;
			else if(time(NULL) -start_time > config.wait_timeout){// timeout arrived
				ret_val=2;
				break;
			}else if(e < 0){
				printf("Error: %s\n",ping_get_error(ping_inst));
				ret_val=1;
				break;
			}
			usleep(config.sleep_time);
		}
	}else{
		DBG("now we wait without timeout");
		while(1){
			register int e= ping_send(ping_inst);
			if(e >=1)// host is up
				break;
			else if(e < 0){
				printf("Error: %s\n",ping_get_error(ping_inst));
				ret_val=1;
				break;
			}
			usleep(config.sleep_time);
		}
	}
	ping_host_remove(ping_inst,host.raw_host);
	ping_destroy(ping_inst);
	return ret_val;
}

int8_t set_ip_addr(void){
	struct addrinfo hints;
	memset(&hints, 0 , sizeof(struct addrinfo));
	
	hints.ai_family = AF_UNSPEC;  
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = 0;
	hints.ai_protocol = 0;   
	if( getaddrinfo(host.raw_host,host.raw_port,&hints,&(host.res)) != 0 || host.res == NULL)
		return 0;
	return 1;
}

int main(int argc, char *argv[]){
	if(argc == 1){
		HELP(NULL);
		return 1;
	}

	int8_t exit_code=0;
	config.sleep_time=200;
	config.ping_timeout=1.0;
	config.wait_timeout=0;
	config.socket_timeout=2000;
	host.res=NULL;
	register int op;
	register long buf;

	while((op=getopt(argc,argv,"hds:1:2:t:")) != -1){
		switch(op){
			case 'h':
				HELP(NULL);
				return 1;
				break;
			case 'd':
				debug=1;
				break;
			case 's':
				if(sscanf(optarg,"%lu",&(config.sleep_time)) != 1){
					HELP("Error: -s: invalid argument!\0");
					exit_code=1;
				}
				break;
			case '1':
				if(sscanf(optarg,"%lf",&(config.ping_timeout)) != 1){
					HELP("Error: -1: invalid argument!\0");
					exit_code=1;
				}
				break;
			case '2':
				if(sscanf(optarg,"%lu",&(config.socket_timeout))!= 1){
					HELP("Error: -2: invalid argument!\0");
					exit_code=1;
				}
				break;

			case 't':
				if(sscanf(optarg,"%lu",&(config.wait_timeout)) != 1){
					HELP("Error: -t: invalid argument!\0");
					exit_code=1;
				}
				break;
			case '?':
				exit_code=1;
				break;
			default:
				exit_code=1;
				HELP(NULL);
		}
	}
	if(debug)
		fprintf(stderr,"[debug] parsed:\n\t sleep_timeout=%lu ping_timeout=%lf socket_timeout=%lu wait_timeout=%lu\n",
			config.sleep_time, config.ping_timeout,config.socket_timeout,config.wait_timeout);	
	if(optind > argc-1){
		HELP("Error: need host\0");
		exit_code=1;
	}else if(optind == argc-2){
		host.raw_port=argv[optind+1];
		host.raw_host=argv[optind];
	}else if(optind == argc-1)
		host.raw_host=argv[optind];
	else{
		HELP("Error: invalid operand range\0");
		exit_code=1;
	}

	if(exit_code != 0)
		return exit_code;
	
	if(!set_ip_addr()){
		HELP("Error: erroneous host (or port)\0");
		exit_code=1;
	}

	if(exit_code == 0){
		if(host.raw_port==NULL){
			if(pingloop() != 0)
				exit_code=1;
		}else{
			if(connectloop() != 0)
				exit_code=1;
		}
	}
	
	freeaddrinfo(host.res);
	return exit_code;
}
