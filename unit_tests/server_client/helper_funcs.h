#ifndef HELPER_FUNCS_H
#define HELPER_FUNCS_H

#include "game_incl.h"

void toLower(unsigned char * word, uint8_t len);
bool wordBeforeWord(unsigned char * word_new, unsigned char * word_old, uint8_t new_n, uint8_t old_n);
bool humanReadable(unsigned char c, bool spaces);

#endif