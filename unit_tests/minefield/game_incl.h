#ifndef GAME_INCL_H
#define GAME_INCL_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <time.h>

#define ASC_NUM 48
#define ASC_LOW 97
#define ASC_UPP 65
#define UIN_LEN 10
#define MAX_SCK 10
#define ETX (unsigned char)0x03
#define SEP (unsigned char)0x20
#define MAX_AUTH_FIELD_LEN 10 // bytes
#define MAX_LRDBRD_ENTRIES 21

#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_TILES NUM_TILES_X * NUM_TILES_Y
#define NUM_MINES 10

#endif