#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "potato.h"
#define MAX_BUF 1024

void removeExsistingFiles(int size){
	char st1[MAX_BUF];
	char st2[MAX_BUF];
	char st3[MAX_BUF];
	char st4[MAX_BUF];
	for(int i = 0; i < size; i++){
		sprintf(st1,"/tmp/master_p%d",i);				
		sprintf(st2,"/tmp/p%d_master",i);
		sprintf(st3,"/tmp/p%d_p%d",i, ((i+1)%size));
		sprintf(st4,"/tmp/p%d_p%d",((i+1)%size),i);
		unlink(st1);
		remove(st1);
		unlink(st2);
		remove(st2);
		unlink(st3);
		remove(st3);
		unlink(st4);
		remove(st4);
		clear_string(st1);
		clear_string(st2);
		clear_string(st3);
		clear_string(st4);
	}
}

void endGame(int * writes, int num){
	POTATO_T * potato = (POTATO_T*)malloc(sizeof(POTATO_T));
	potato->msg_type = 'x'; // x means end game
	for(int i = 0; i < num; i++){
		write(writes[i],potato,sizeof(POTATO_T));
	}
	//removeExsistingFiles(num);	
	exit(0);
}

int main(int argc, char *argv[])
{
	if(argc != 3){
		printf("Invalid inputs, Expected: ./ringmaster <num_players> <num_hops>\n");
		return EXIT_FAILURE;
	}

	int num_players = atoi(argv[1]);
	int num_hops = atoi(argv[2]);
	
	//Check for valid number of players
	char buf[MAX_BUF];
	int fd_read[num_players];
	int fd_write[num_players];

	removeExsistingFiles(num_players);
	if(num_players <= 1){
		printf("Must have more than 1 player\n");
		char cmd[100];	
		char k[100];
		strcpy(k,"pkill player");
		sprintf(cmd,"/tmp/master_p%d",0);				
		mkfifo(cmd, 0666);	
		int fd = open(cmd, O_WRONLY);
		int kill = -1;
		write(fd, &kill, sizeof(int));
		system(k);
		exit(0);
	}

	char rm_read[MAX_BUF];
	char rm_write[MAX_BUF];
	char player0[MAX_BUF];
	char player1[MAX_BUF];
	
		
	// Set up ALL FIFOS for communication
	for(int i = 0; i < num_players; i++){
		sprintf(rm_write,"/tmp/master_p%d",i);				
		//printf("%s\n", rm_write);
		sprintf(rm_read,"/tmp/p%d_master",i);
		//printf("%s\n", rm_read);
		sprintf(player0,"/tmp/p%d_p%d",i, ((i+1)%num_players));
		//printf("%s\n",player0);
		sprintf(player1,"/tmp/p%d_p%d",((i+1)%num_players),i);
		//printf("%s\n",player1);
		mkfifo(rm_write, 0666);	
		mkfifo(rm_read, 0666);
		mkfifo(player0, 0666);
		mkfifo(player1, 0666);
		clear_string(rm_write);
		clear_string(rm_read);
		clear_string(player0);
		clear_string(player1);
	}			

	for(int j = 0; j < num_players; j++){
		sprintf(rm_write,"/tmp/master_p%d",j);				
		//printf("writing to fifo %s....\n", rm_write);	
		fd_write[j] = open(rm_write, O_WRONLY);
		write(fd_write[j], &num_players, sizeof(int));
		
		sprintf(rm_read,"/tmp/p%d_master",j);
		//printf("reading from fifo %s....\n", rm_read);	
		fd_read[j] = open(rm_read, O_RDONLY);
		read(fd_read[j], buf, MAX_BUF);	
		if(strcmp(buf,"ready") == 0){
			printf("Player %d is ready to play\n",j);
		}else{
			printf("ERROR: player %d didn't return ready\n",j);
			return EXIT_FAILURE;
		}
		
		clear_string(rm_write);
		clear_string(rm_read);
	}
/*
*	Lets get ready to start the game!
*	
*	1) Generate Random Number
*	2) Pass the potato!
*	3) Wait to get it back :)
*
*/
	// Create potato!
	POTATO_T * potato = (POTATO_T*)malloc(sizeof(POTATO_T));
	potato->msg_type = 'y'; // y means play game!
	potato->total_hops = num_hops;
	potato->hop_count = 0;
	//Seed random num generator
	time_t t;
	srand((unsigned) time(&t));
	int first = rand()%num_players;
	if(num_hops >= 0 || num_hops > 512){	
		if(num_hops > 512){
			printf("All players present, greater than 512 hops. Ends game\n");
			endGame(fd_write,num_players);
		}else if(num_hops > 0){
			printf("All players present, sending potato to player %d\n",first);
			write(fd_write[first], potato, sizeof(POTATO_T));
		}else{
			printf("All players present, 0 hops so ending game\n");
			endGame(fd_write,num_players);
		}
	}else{
		printf("number of hops must be [0,512]\n");
		// TODO: kill all players
		endGame(fd_write,num_players);
	}
		
	// Wait to get the potato back and print trace.
	fd_set rfds;
	struct timeval tv;
	int returnVal;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	while(1){
		FD_ZERO(&rfds);
		for(int i = 0; i < num_players; i++){
			FD_SET(fd_read[i],&rfds);
		}		
		int idx = -1;
		returnVal = select(FD_SETSIZE, &rfds, NULL,NULL,&tv);
		if(returnVal == -1){
				printf("select failed\n");
		}else if(returnVal){
			for(int i = 0; i <num_players; i++){
				if(FD_ISSET(fd_read[i],&rfds)){
					//printf("found isset at player %d\n",i);
					idx = i;
					//break;
				}	
			}
			//printf("idx = %d\n",idx);
			if(idx >= 0){ // we say we found an ISSET
				int	sz = read(fd_read[idx],potato,sizeof(POTATO_T));
				if(sz > 0){
					//printf("Pass from player %d to master\n",idx);
					//printf("Read %d bytes.\n", sz);
					//printf("msg: %c, t_hops:%d, hopc: %d\n",potato->msg_type,potato->total_hops,potato->hop_count);
					printf("Trace of potato:\n");	
					for(int i = 0; i < num_hops; i++){
						if(i == num_hops-1){
							printf("%lu",potato->hop_trace[i]);
						}else{
							printf("%lu,",potato->hop_trace[i]);
						}
					}
					printf("\n");
					endGame(fd_write,num_players);
				}
			}
		}else{
			printf("master timeout\n");
		}
	}
	exit(0);
}

