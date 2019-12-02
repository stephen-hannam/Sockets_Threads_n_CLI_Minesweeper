#include "helper_funcs.h"

// only caring about alphabet letters, numbers, and spaces or not spaces
bool humanReadable(unsigned char c, bool spaces){
    if(spaces){
        if((c >= '0' && c <= '9') \
            || (c >= 'A' && c <= 'Z') \
            || (c >= 'a' && c <= 'z') \
            || c == SEP) return true;
    }
    else if((c >= '0' && c <= '9')  \
        || (c >= 'A' && c <= 'Z') \
        || (c >= 'a' && c <= 'z')) return true;
    return false; 
}

void toLower(unsigned char * word, uint8_t len){
    for(uint8_t t = 0; t < len; t++)
        if(word[t] >= 0x41 && word[t] <= 0x5A) word[t] += 0x20;
}

bool wordBeforeWord(unsigned char * word_new, unsigned char * word_old, uint8_t new_n, uint8_t old_n){
    uint8_t t = 0;
    unsigned char letter_new = word_new[t];
    unsigned char letter_old = word_old[t];
    while(letter_old == letter_new){
        t++;
        letter_new = word_new[t];
        letter_old = word_old[t]; 
    }
    if(old_n == new_n && t == old_n) return false;
    if(letter_new < letter_old) return true;
    return false;
}