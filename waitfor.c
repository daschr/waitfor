#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
extern int h_errno;
#include <oping.h>

#define CON_SLEEP_TIME 1
#define CUS_PING_SLEEP 200
#define CUS_PING_TIMEOUT 1
#define HELP(X) helpmsg(argv[0],X)

struct host{
	char *raw_host;
	void *ip;
	struct sockaddr *addr;
	socklen_t addrlen;
	int ip_type;
	unsigned int port;
	unsigned long wait_timeout;
};


void helpmsg(char *pname,char *s){
	printf("Usage: %s ?[-t[timeout]] [host] ?[tcp port]\n",pname);
	if(s != NULL){
		fprintf(stderr,"%s",s);
	}
	fprintf(stderr,"\n");
}


int8_t connectloop(struct host **h){
	int socket_d;
	int8_t ret_val=0;

	socket_d=socket((*h)->ip_type,SOCK_STREAM,0);
	if(socket_d == -1){
		fprintf(stderr,"Error: could not create socket!\n");
		return 1;
	}
	if((*h)->wait_timeout != 0){
		time_t start_time=time(NULL);
		while(connect(socket_d,(*h)->addr,(*h)->addrlen)<0){
			if(time(NULL) - start_time > (*h)->wait_timeout){ 
				ret_val=2;
				break;
			}
			sleep(CON_SLEEP_TIME);
		}
	}else
		while(connect(socket_d,(*h)->addr,(*h)->addrlen)<0)
			sleep(CON_SLEEP_TIME);
		
	close(socket_d);
	return ret_val;
} 


int8_t pingloop(struct host **h){
	long timeout=CUS_PING_TIMEOUT;
	int8_t ret_val=0;

	pingobj_t *ping_inst= ping_construct();
	if(ping_inst == NULL){
		fprintf(stderr,"Error: could not instanciate ping!\n");
		free(ping_inst);
		return 1;
	}
	if(ping_setopt(ping_inst,PING_OPT_TIMEOUT,&timeout) != 0){
		fprintf(stderr,"%s\nError: could not instanciate ping timeout!\n",ping_get_error(ping_inst));
		free(ping_inst);
		return 1;
	}
	if(ping_setopt(ping_inst,PING_OPT_AF,&((*h)->ip_type)) != 0){
		fprintf(stderr,"%s\nError: could not set af type!\n",ping_get_error(ping_inst));
		free(ping_inst);
		return 1;
	}
	if(ping_host_add(ping_inst,(*h)->raw_host) != 0){
		fprintf(stderr,"%s\nError: could not instanciate host to ping!\n",ping_get_error(ping_inst));
		free(ping_inst);
		return 1;
	}
	if((*h)->wait_timeout != 0){
		time_t start_time=time(NULL);
		while(1){
			register int e= ping_send(ping_inst);
			if(e >=1) //host is up
				break;
			else if(time(NULL) -start_time > (*h)->wait_timeout){// timeout arrived
				ret_val=2;
				break;
			}else if(e < 0){
				printf("%s\n",ping_get_error(ping_inst));
				ret_val=1;
				break;
			}
			usleep(CUS_PING_SLEEP);
		}
	}else{
		while(1){
			register int e= ping_send(ping_inst);
			if(e >=1)// host is up
				break;
			else if(e < 0){
				printf("%s\n",ping_get_error(ping_inst));
				ret_val=1;
				break;
			}
			usleep(CUS_PING_SLEEP);
		}
	}
	ping_host_remove(ping_inst,(*h)->raw_host);
	free(ping_inst);
	return ret_val;
}

int8_t set_ip_addr(struct host **host,char *s,char *p){
	(*host)->raw_host=s; // since it's just used with argv, it's ok
	struct addrinfo hints;
	struct addrinfo	*res;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;  
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = 0;
	hints.ai_protocol = 0;   
	
	if( getaddrinfo(s,p,&hints,&res) != 0 || res == NULL)
		return 0;
	(*host)->addr=res->ai_addr;
	(*host)->addrlen=res->ai_addrlen;
	(*host)->ip_type=res->ai_family;
	
	return 1;
}

int8_t parse_timeout(struct host **host,char *s){
	unsigned long timeout;
	if(sscanf(s,"-t%lu",&timeout) == 1){
		(*host)->wait_timeout=timeout;
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[]){
	struct host *h=malloc(sizeof(struct host));	
	int8_t exit_code=0;

	switch(argc){
		case 1:
			helpmsg(argv[0],"Error: no arguments given!\0");
			break;
		case 2:
			h->wait_timeout=0;
			if(set_ip_addr(&h,argv[1],NULL)){
				if(pingloop(&h) != 0){
					exit_code=1;
				}
			}else
				HELP("Error: erroneous host\0");
			break;
		case 3:
			h->wait_timeout=0;
			if(parse_timeout(&h,argv[1])){
				if(set_ip_addr(&h,argv[2],NULL)){
					if(pingloop(&h) != 0){
						exit_code=1;
					}
				}else 
					HELP("Error: erroneous host\0");
			}else{
				if(set_ip_addr(&h,argv[1],argv[2])){
					if(connectloop(&h) != 0){
						exit_code=1;
					}
				}else 
					HELP("Error: erroneous host or port\0");
			}	
			break;
		case 4:
			h->wait_timeout=0;
			if(parse_timeout(&h,argv[1])){
				if(set_ip_addr(&h,argv[2],argv[3])){
					if(connectloop(&h) != 0){
						exit_code=1;
					}
				}else 
					HELP("Error: erroneous host or port\0");
			}else
					HELP("Error: invalid argument!\0");
				
			break;
		default:
			helpmsg(argv[0],"Error: more arguments than expected!\0");
	}
	free(h);
	return exit_code;
}
