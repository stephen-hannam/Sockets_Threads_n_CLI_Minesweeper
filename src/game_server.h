#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include "game_incl.h"
#include "game_engine.h"
#include "helper_funcs.h"
#include "packets.h"

#include <pthread.h>

#define ISSERVER 1

#define RAND_NUM_SEED 42

#define SERVER_LISTEN_PORT 65535

#define MAX_PLAYERS 10
#define BACKLOG 10     /* how many pending connections queue will hold */
#define NUM_THREADS 3
// codes returned by attempt to acquire leaderboard mutex
#define LMUT_OK 0
#define LMUT_BREAK_CLEAN 1
#define LMUT_BREAK_AGAIN 2

/* player info struct */
typedef struct{
	uint8_t id; // based on idx pos in auth table == player_num
	int fd; // file descriptor for socket being used by this player
	unsigned char * name;
	unsigned char * pword;
	unsigned char ** part_map;
	int8_t ** map;
	uint8_t mines_rem;
	time_t start_time;
	struct drand48_data * randBuffer;
}client_info;

typedef struct request{
    client_info * player; // &players[n]
    struct request * next;
}request_t;

/* Thread pool functions */
void * handleReqsLoop(void);
void handleReq(request_t * a_request);
request_t * getReq(pthread_mutex_t * p_mutex);
void addReq(client_info * player_s);
/* Data exchange functions */
int8_t listenOnSocket(int * sockfd, uint16_t port);
int refreshFdSets(fd_set * read_fds, int sockfd, client_info * players);
void acceptNewConnection(client_info * players, int sockfd);
int8_t authConnection(client_info * players, int new_fd);
void sendLeader(int fd, uint8_t pnum);
/* Helper functions */

void destroyPlayers(client_info * players);
void initPlayers(client_info * players);

void initMap(int8_t ** map, struct drand48_data * randBuffer);
void clearMaps(client_info * player);

#endif