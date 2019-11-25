#ifndef PACKETS_H
#define PACKETS_H

#include "game_incl.h"
#include "helper_funcs.h"

#include <sys/ioctl.h> 
#include <netdb.h> 
#include <netinet/in.h>  
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#define NO_SOCKET -1
#define RET_ERR -1
#define AUTH_FAIL -1

#define RESEND_LIM 6
#define MAX_PLAYERS 10

#define CONN_ERR -1
#define CONN_OFF 0
#define STD_PCKT 1
#define CHIRP 2
#define INVLD_PCKT 3

#define NUM_LU_CHARS 20 + 1 // + SEP
#define NUM_U16_CHARS 5 + 1 // + SEP
#define NUM_U8_CHARS 3 + 1 // + SEP
#define NUM_ENTRIES_CHARS 2 + 1 // + SEP

#define CHIRP_LEN 4
#define CHIRP_LEN0 0
#define CHIRP_LEN1 1
#define CHIRP_NAME 2
#define CHIRP_DATA 3 // optional

#define NUM_FRAME_BYTES 5
#define PCKT_LEN0 0
#define PCKT_LEN1 1
#define PCKT_ID 2
#define PCKT_VERB 3
#define PCKT_DATA 4
#define MAX_STAT_SIZE 4 // bytes
#define TIME_SIZE 8 // bytes, but is environment dependent

#define MAX_PACKET_LEN MAX_LRDBRD_ENTRIES*(2*NUM_U16_CHARS + MAX_AUTH_FIELD_LEN + NUM_LU_CHARS) + NUM_FRAME_BYTES + NUM_ENTRIES_CHARS

typedef struct{
	unsigned char verb;
	unsigned char * data;
	uint16_t data_len;
}prev_packet;

bool validChirp(unsigned char C);
void sendChirp(int fd, unsigned char name, void * player_num);
bool validPacket(bool new_conn, bool isserver, unsigned char * packet, uint16_t exp_len);
int8_t getPacket(bool new_conn, bool isserver, int fd, unsigned char packet[MAX_PACKET_LEN]);
void sendPacket(int fd, uint8_t player_num, volatile prev_packet * prev_pckt, unsigned char verb,\
			    unsigned char * data, uint16_t data_len);
void initPrevPckt(volatile prev_packet * prev_pckt);

#endif

/* 
* Format for packet; Length, player_num*, verb*, {data}, ETX
* 					 SEP separates data fields in {data}
* length in PCKT_LEN0 and PCKT_LEN1: 16 bit total
*									 LEN0 -> LSB, LEN1 -> MSB
* *: only the first authorisation packet a client sends may exclude player_num and verb
*    prior and next send from server are also 1-byte ('?', and then [player_num])
*
* Allowed verbs: S, L, Q, q, P, R, M/D, C, X, Y, W
* ------- Allowed client verbs (client to send) -------------
* S = start game 				-> strictly no data
* L = request leader board  	-> strictly no data
* Q = quit whole game 			-> strictly no data
* q = quit current minefield	-> strictly no data
* P = place flag 				-> must be accompanied by data: "x,y" 
* R = reveal tile 				-> must be accompanied by data: "x,y" 
* ------- Allowed server verbs (server to send) -------------
* S = confirm start	-> strictly no data
* M/D = map/dead 	-> always accompanied by 81 byte part_map
*				 	-> M = revealed tile was a zero, send new part_map to client
*				 	-> D = revealed tile was a mine, send new part_map with all mines shown
* C = coordinate 	-> always accompanied by 3 byte "x,y,c" data
* 				 	-> tells client what value (c) is under revealed tile at (x,y)
* X = no mine    	-> disallowed flag placement @ (x,y)
* Y = flag placed	-> flag placement successful @ (x,y)
* W = you won    	-> self-explanatory, data = game time
* L = leaderboard	-> data to follow is entire leader-board
* q = confirm quit	-> server confirms client quit minefield, server deallocs that game
*
* Allowed data fields: {"SEP"}, ["x,y"], ["x,y,c"], ["[part_map]"] 81 fields, ["[leader_brd]"]
* 

* CHIRPS: single character sent after a '0' in length field 
* used to quickly indicate state changes in cases where sending/getting
* full packets might not be appropriate, NB: '0' == [0x00, 0x00] in ASCII
* 
* Special server-side CHIRPs:
* 0,A,n = client logon accepted, n = player_num assigned by server
* 0,? = who are you? 
* 0,D = pword or name incorrect
* 0,* = server SIGINT
* 0,| = client already logged on at another terminal
* Bidirectional CHIRP: 
* 0,+ = non-fatal problem, resend last packet
*/