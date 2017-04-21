#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "potato.h"
#define MAX_BUF 1024
#define NUM_NEIGHBORS 3

void passPotato(int fd, POTATO_T * p){
	write(fd, p, sizeof(POTATO_T));	
}

void killPlayer(int id){
	char rfm[MAX_BUF];
	char wtm[MAX_BUF];	
	sprintf(rfm,"/tmp/master_p%d",id);
	sprintf(wtm,"/tmp/p%d_master",id);	
	unlink(rfm);
	unlink(wtm);
	exit(0);
}

int main(int argc, char *argv[])
{
	if(argc !=2){
		printf("Invalid inputs, Expected: ./player <player_id>\n");
		return EXIT_FAILURE;
	}
	
	int id = atoi(argv[1]);
	int num_players;
	char buf[MAX_BUF];
	char readFromMaster[MAX_BUF];
	char writeToMaster[MAX_BUF];
	char l_read[MAX_BUF];
	char l_write[MAX_BUF];
	char r_read[MAX_BUF];
	char r_write[MAX_BUF];	
	time_t t;

	// Set up connections to and from the Ring Master	
	sprintf(readFromMaster,"/tmp/master_p%d",id);

	// If file doesn't exist lets kill player process!

	sprintf(writeToMaster,"/tmp/p%d_master",id);	
	//printf("player %d, reading from %s\n",id,readFromMaster);
	int fd_rMaster = open(readFromMaster, O_RDONLY);	
	read(fd_rMaster,&num_players,sizeof(int));
	if(num_players == -1){
		killPlayer(id);
	}
	printf("Connected as player %d out of %d total players\n",id,num_players);
	
	int fd_wMaster = open(writeToMaster, O_WRONLY);			
	write(fd_wMaster,"ready",sizeof(buf));
	
	//printf("Player %d ready to set up player connections\n",id);
	// Set up connections with neighbor players
	int fd_reads[3];
	int fd_writes[3];
	if(id == 0){
		//printf("Player %d is the first player\n",id);
		// First player
		// 1. Read from +1
		sprintf(r_read,"/tmp/p%d_p%d",((id+1)%num_players),id);
		fd_reads[0] = open(r_read, O_RDONLY);
		//printf("first read from %d\n",((id+1)%num_players));
		// 2. Write to + 1
		sprintf(r_write,"/tmp/p%d_p%d",id,((id+1)%num_players));
		fd_writes[0] = open(r_write, O_WRONLY);
		//printf("first write to %d\n",((id+1)%num_players));

		// 3. Read Last
		sprintf(l_read,"/tmp/p%d_p%d",(num_players-1),id);
		fd_reads[1] = open(l_read, O_RDONLY);
		//printf("first read from %d\n",(num_players-1));

		// 4. Write Last
		sprintf(l_write,"/tmp/p%d_p%d",id,(num_players-1));
		fd_writes[1] = open(l_write, O_WRONLY);
		//printf("first write to %d\n",(num_players-1));

		clear_string(r_write);
		clear_string(r_read);
		clear_string(l_write);
		clear_string(l_read);

	}else if(id == num_players-1){
		// Last player
		//printf("Player %d is the last player\n",id);
		// 1. Write to 0
		sprintf(r_write,"/tmp/p%d_p%d",id,((id+1)%num_players));
		fd_writes[0] = open(r_write, O_WRONLY);
		//printf("last write to %d\n",((id+1)%num_players));
		// 2. Read from 0
		sprintf(r_read,"/tmp/p%d_p%d",((id+1)%num_players),id);
		fd_reads[0] = open(r_read, O_RDONLY);
		//printf("last read to %d\n",((id+1)%num_players));
		// 3. Write to -1
		sprintf(l_write,"/tmp/p%d_p%d",id,((id-1)%num_players));
		fd_writes[1] = open(l_write, O_WRONLY);
		//printf("last write to %d\n",((id-1)%num_players));
		// 4. Read from -1
		sprintf(l_read,"/tmp/p%d_p%d",((id-1)%num_players),id);
		fd_reads[1] = open(l_read, O_RDONLY);
		//printf("last read to %d\n",((id-1)%num_players));

		clear_string(r_write);
		clear_string(r_read);
		clear_string(l_write);
		clear_string(l_read);
	}else{
		// Every other player
		//printf("Player %d is a middle player\n",id);
		// 1. Write to -1
		sprintf(l_write,"/tmp/p%d_p%d",id,((id-1)%num_players));
		fd_writes[1] = open(l_write, O_WRONLY);
		//printf("player %d write to %d\n",id,((id-1)%num_players));
		// 2. Read from -1	
		sprintf(l_read,"/tmp/p%d_p%d",((id-1)%num_players),id);
		fd_reads[1] = open(l_read, O_RDONLY);
		//printf("player %d read %d\n",id,((id-1)%num_players));
		// 3. Read from +1
		sprintf(r_read,"/tmp/p%d_p%d",((id+1)%num_players),id);
		fd_reads[0] = open(r_read, O_RDONLY);
		//printf("player %d read %d\n",id,((id+1)%num_players));
		// 4. Write to +1
		sprintf(r_write,"/tmp/p%d_p%d",id,((id+1)%num_players));
		fd_writes[0] = open(r_write, O_WRONLY);
		//printf("player %d write %d\n",id,((id+1)%num_players));

		clear_string(r_write);
		clear_string(r_read);
		clear_string(l_write);
		clear_string(l_read);
	}	
	
	fd_writes[2] = fd_wMaster;
	fd_reads[2] = fd_rMaster;
	// All communication channels done. Time to start the game!
	fd_set rfds;	
	fd_set wfds;
	struct timeval tv;
	int returnVal;
	tv.tv_sec = 2;
	tv.tv_usec = 0; 
	srand((unsigned) time(&t) + id);
	POTATO_T * p = (POTATO_T*)malloc(sizeof(POTATO_T));

	while(1){
		FD_ZERO(&rfds);
		FD_SET(fd_reads[0],&rfds);
		FD_SET(fd_reads[1],&rfds);
		FD_SET(fd_reads[2],&rfds);
		returnVal = select(FD_SETSIZE, &rfds, NULL,NULL,&tv);
		if(returnVal == -1){
			perror("Select Failed\n");
		}else if(returnVal){
			//Figure out who did the writing
			if(FD_ISSET(fd_reads[0],&rfds)){
				// Pass from our neighbor +1
				int sz = read(fd_reads[0],p,sizeof(POTATO_T));
				if(sz > 0){
					//printf("pass from %d to %d\n",(id+1)%num_players,id);
					p->hop_trace[p->hop_count] = id;
					p->hop_count++;
					if(p->total_hops == p->hop_count && p->msg_type == 'y'){
						//printf("1 hop return to master,p%d\n",id);
						printf("I'm it\n");
						passPotato(fd_writes[2], p);
					}else{
						// Pass the potato!
						int toss = rand()%2;	
						//printf("toss: %d for p%d\n",toss,id);
						if(toss){
							int next;
							if(id == 0){next = num_players-1;}
							else{next = id-1;}
							printf("Sending potato to %d\n",next);
						}else{
							int next;
							if(id == num_players-1){
								next = 0;
							}else{next = id+1;}
							printf("Sending potato to %d\n",next);
						}
						passPotato(fd_writes[toss],p);
					}
				}
				
			}else if(FD_ISSET(fd_reads[1], &rfds)){
				// Pass from neighbor -1
				int sz = read(fd_reads[1],p,sizeof(POTATO_T));
				if(sz > 0){
					//printf("pass from %d to %d\n",(id-1)%num_players,id);
					p->hop_trace[p->hop_count] = id;
					p->hop_count++;
					if(p->total_hops == p->hop_count && p->msg_type == 'y'){
						//printf("1 hop return to master,p%d\n",id);
						printf("I'm it\n");
						passPotato(fd_writes[2], p);
					}else{
						// Pass the potato!
						int toss = rand()%2;	
						//printf("toss: %d for p%d\n",toss,id);
						if(toss){
							int next;
							if(id == 0){next = num_players-1;}
							else{next = id-1;}
							printf("Sending potato to %d\n",next);
						}else{
							int next;
							if(id == num_players-1){
								next = 0;
							}else{next = id+1;}
							printf("Sending potato to %d\n",next);
						}
						passPotato(fd_writes[toss],p);
					}
				}
			}else if(FD_ISSET(fd_reads[2], &rfds)){
				// Pass from RM
				//FD_CLR(fd_reads[2],&rfds);
				int sz = read(fd_reads[2],p,sizeof(POTATO_T));
				if(sz > 0){
					//printf("Pass from RM to player %d\n",id);
					//printf("Read %d bytes from master\n", sz);
					//printf("msg: %c, t_hops:%d, hopc: %d\n",p->msg_type,p->total_hops,p->hop_count);
					//if hop_count == 0 return to master
					p->hop_trace[p->hop_count] = id;
					p->hop_count++;
					//printf("increase hopc + 1 = %d\n",p->hop_count);
					if(p->total_hops == p->hop_count && p->msg_type == 'y'){
						//printf("1 hop return to master,p%d\n",id);
						passPotato(fd_writes[2], p);
					}else if(p->msg_type == 'x'){
						//time to end
						//printf("killing player %d\n",id);
						killPlayer(id);
						exit(0);
					}else{
						// Pass the potato!
						int toss = rand()%2;	
						//printf("toss: %d for p%d\n",toss,id);
						if(toss){
							int next;
							if(id == 0){next = num_players-1;}
							else{next = id-1;}
							printf("Sending potato to %d\n",next);
						}else{
							int next;
							if(id == num_players-1){
								next = 0;
							}else{next = id+1;}
							printf("Sending potato to %d\n",next);
						}
						passPotato(fd_writes[toss],p);
					}
				}
			}
		}else{
			//printf("No data written to pipe %d in 2 last seconds.\n",id);
		}

	}
	exit(0);
}
