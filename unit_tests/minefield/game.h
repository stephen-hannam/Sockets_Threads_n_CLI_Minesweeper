#ifndef GAME_H_
#define GAME_H_

#include "game_incl.h"
#include "helper_funcs.h"

#define MINE INT8_MIN
#define FLAG 0
#define LBL_OFFSET 9

#define DEAD -1
#define INVLD 0
#define BLOB 0
#define VLD 1

#define RAND_NUM_SEED 42

void test_displayMap(int8_t ** map);
void initMap(int8_t ** map, struct drand48_data * randBuffer);
void placeMines(int8_t ** map, struct drand48_data * randBuffer);
void placeAdjs(int8_t ** map);
void blobRelabel(int8_t ** map, int8_t old, int8_t new);
void newLabel(int8_t ** map, int8_t * tiles, uint8_t count);
void addBlobFringes(int8_t ** map);
void placeBlobs(int8_t ** map);

#endif
