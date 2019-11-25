#ifndef GAME_CLIENT_H_
#define GAME_CLIENT_H_

#include "game_incl.h"
#include "helper_funcs.h"
#include "packets.h"

#include <signal.h>

#define ISSERVER 0

#define TOTAL_WIDTH 90

unsigned char welcome(void);
void displayMap(unsigned char ** part_map, uint8_t rem);
unsigned char getPlayerMove(unsigned char ** part_map, uint8_t * x, uint8_t * y);
void gameController(int sockfd, uint8_t id);
int8_t gameOver(bool dead);
void printLeader(unsigned char * curr_lbrd);
void userLogIn(int * sockfd, uint8_t * id, char ** argv);
void setupConnection(int * sockfd, char ** argv);
void shutdownClient(int fd, unsigned char ** part_map);
static void handleSIGINT(int sig);
/* adviceX functions implement a simple chirp-based quasi-hand-shake */
// these fnuctions must be given a pointer to the packet or chirp that is
// expected in return from message being sent at the start of adviseX function
void adviseAuth(int fd, unsigned char chirp[CHIRP_LEN], unsigned char auth_data[2*MAX_AUTH_FIELD_LEN + 1],\
              uint16_t r_data_len);
void adviseMove(int fd, uint8_t id, unsigned char verb, uint8_t x, uint8_t y,\
              unsigned char packet[MAX_PACKET_LEN], unsigned char ** part_map);

#endif
