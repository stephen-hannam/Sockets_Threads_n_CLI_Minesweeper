#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "game_incl.h"
#include "helper_funcs.h"
#include "packets.h"

#include <signal.h>

#define MINE INT8_MIN
#define FLAG 0
#define LBL_OFFSET 9

#define DEAD -1
#define INVLD 0
#define BLOB 0
#define VLD 1


typedef struct{
	uint8_t num_entries; // MAX = 21 = MAX_LRDBRD_ENTRIES
	uint8_t * best_ids; 
	time_t * best_times; // IT WAS THE BEST OF TIMES, IT WAS THE BLURST OF TIMES! stupid monkey
	uint16_t * player_stats; // {num_won, num_played, ...} @ idx1 = 2*id, idx2 = 2*id + 1 
	unsigned char ** player_names; // len = MAX_PLAYERS * MAX_AUTH_FIELD_LEN, idx = id
	bool vld; // for error checking and backups
}leaderbrd;

void placeMines(int8_t ** map, struct drand48_data * randBuffer);
void placeAdjs(int8_t ** map);
void blobRelabel(int8_t ** map, int8_t old, int8_t new);
void newLabel(int8_t ** map, int8_t * tiles, uint8_t count);
void addBlobFringes(int8_t ** map);
void placeBlobs(int8_t ** map);
void revealBlob(int8_t ** map, unsigned char ** part_map, int8_t id);
int8_t updateMap(int8_t ** map, unsigned char ** part_map, uint8_t x, uint8_t y, bool place_flag);

void pullAuthFields(unsigned char ** names, unsigned char ** pwords);
void initLeader(volatile leaderbrd * lbrd, bool restore, volatile leaderbrd * BAK, bool * bail);
void destroyLeader(volatile leaderbrd * lbrd);
void updateLeader(volatile leaderbrd * lbrd, volatile leaderbrd * prev_lbrd, uint8_t pnum, time_t start_game,\
			      bool won, bool force_sort);
bool restorePrevLeader(volatile leaderbrd * a_lbrd, volatile leaderbrd * b_lbrd);
void backupLeader(volatile leaderbrd * lbrd, volatile leaderbrd * prev_lbrd);

#endif