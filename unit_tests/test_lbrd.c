#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define ASC_NUM 48
#define ASC_LOW 97
#define ASC_UPP 65
#define UIN_LEN 10
#define MAX_SCK 10
#define ETX 0x03
#define SEP 0x20

#define MAX_AUTH_FIELD_LEN 10 // bytes
#define MAX_LRDBRD_ENTRIES 21
#define MAX_PLAYERS 10

#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_TILES NUM_TILES_X * NUM_TILES_Y
#define NUM_MINES 10

#define SERVER_LISTEN_PORT 33333
#define SERVER_IPV4_ADDR "127.0.0.1"
#define NO_SOCKET -1
#define RET_ERR -1
#define AUTH_FAIL -1
#define SERVER_NAME "game_server"

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

static char const * const AUTH_FILE_NAME = "Authentication.txt";

typedef struct{
    uint8_t num_entries; // MAX = 21 = MAX_LRDBRD_ENTRIES
    uint8_t * best_ids; 
    time_t * best_times; // IT WAS THE BEST OF TIMES, IT WAS THE BLURST OF TIMES! stupid monkey
    uint16_t * player_stats; // {num_won, num_played, ...} @ idx1 = 2*id, idx2 = 2*id + 1 
    unsigned char ** player_names; // len = MAX_PLAYERS * MAX_AUTH_FIELD_LEN, idx = id
    bool vld; // for error checking and backups
}leaderbrd;
static volatile leaderbrd * lbrd;
static volatile leaderbrd * prev_lbrd;

void toLower(unsigned char * word, uint8_t len){
    for(uint8_t t = 0; t < len; t++)
        if(word[t] >= 0x41 && word[t] <= 0x5A) word[t] += 0x20;
}

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

bool wordIsWord(unsigned char * word1, unsigned char * word2, uint8_t n1, uint8_t n2){
    if(n1 != n2) return false;
    for(uint8_t t = 0; t < n1; t++) if(word1[t] != word2[t]) return false;
    return true;
}

void pullAuthFields(unsigned char ** names, unsigned char ** pwords){
    FILE * auth;
    if(!(auth = fopen(AUTH_FILE_NAME,"r"))){
        perror("Authentication error, check file name, etc");
        if(kill(getpid(), SIGINT)) exit(EXIT_FAILURE);
    }

    // pull authentication fields from Authentication.txt
    unsigned char temp[MAX_AUTH_FIELD_LEN];
    uint8_t n = 0; uint8_t m = 0;
    while (fscanf(auth, "%9s", temp) == 1){ 
        n++;
        if(n > 2){
            m = (n - 1) % 2;
            for(uint8_t p = 0; p < MAX_AUTH_FIELD_LEN; p++){
                if(m == 0) 
                    names[(n-m-3)/2][p] = (humanReadable(temp[p], false)) ? \
                                           temp[p] : 0x00;
                else if(pwords != NULL)
                    pwords[(n-m-3)/2][p] = (humanReadable(temp[p], false)) ? \
                                            temp[p] : 0x00;
            } 
        }
    }
    fclose(auth);
}

void initLeader(volatile leaderbrd * lbrd, bool restore, volatile leaderbrd * BAK, bool * bail){

    bool nonrecover = false;
    if(bail != NULL) *bail = false;

    uint8_t * best_ids;
    if(!(best_ids = malloc(MAX_LRDBRD_ENTRIES * sizeof(*best_ids)))){
        perror("failed to allocate best_ids");
        nonrecover = true;
    }
    
    time_t * best_times;
    if(!(best_times = malloc(MAX_LRDBRD_ENTRIES * sizeof(*best_times)))){
        perror("failed to allocate best_times");
        nonrecover = true;   
    }

    uint16_t * player_stats;
    if(!(player_stats = malloc(2 * MAX_PLAYERS * sizeof(*player_stats)))){
        perror("failed to allocate player_stats");
        nonrecover = true;      
    }

    unsigned char ** names;
    if(names = malloc(MAX_PLAYERS * sizeof(*names))){
        for(uint8_t n = 0; n < MAX_PLAYERS; n++){
            if(!(names[n] = malloc(MAX_AUTH_FIELD_LEN * sizeof(**names)))){
                fprintf(stderr, "\nfailed to allocate names[%d]\n", n);
                nonrecover = true;  
            } 
        }
    }
    else{
        perror("failed to allocate names");
        nonrecover = true;       
    } 

    pullAuthFields(names, NULL);

    if(!restore){
        lbrd->num_entries = 0;
        lbrd->best_ids = best_ids;
        lbrd->best_times = best_times;
        lbrd->player_stats = player_stats;
        lbrd->player_names = names;
        for(uint8_t n = 0; n < MAX_PLAYERS; n++) 
            lbrd->player_names[n] = names[n];
        lbrd->vld = true;
    }
    else if(BAK != NULL){
        lbrd->num_entries = BAK->num_entries;
        lbrd->best_ids = BAK->best_ids;
        lbrd->best_times = BAK->best_times;
        lbrd->player_stats = BAK->player_stats;
        lbrd->player_names = BAK->player_names;
        for(uint8_t n = 0; n < MAX_PLAYERS; n++) 
            lbrd->player_names[n] = BAK->player_names[n];
        lbrd->vld = BAK->vld;   
    }
    else if(BAK == NULL || nonrecover){
        perror("backup leader board memory lost");
        if(kill(getpid(), SIGINT)) exit(EXIT_FAILURE);
        if(bail != NULL) *bail = true;
    }
}

void destroyLeader(volatile leaderbrd * lbrd){
    
    for(uint8_t n = 0; n < MAX_PLAYERS; n++){
        free(lbrd->player_names[n]);
    }
    free(lbrd->player_names);
    free(lbrd->best_ids);
    free(lbrd->best_times);
    free(lbrd->player_stats);
}

bool restorePrevLeader(volatile leaderbrd * a_lbrd, volatile leaderbrd * b_lbrd){
    bool bail = false;
    destroyLeader(a_lbrd);
    initLeader(a_lbrd, true, b_lbrd, &bail);
    // SIGINT could be broadcast from initLeader
    if(bail) return true; 
    return false;
}

void backupLeader(volatile leaderbrd * lbrd, volatile leaderbrd * prev_lbrd){
    prev_lbrd->vld = false; //if thread dies before end, won't be set back to true

    prev_lbrd->num_entries = lbrd->num_entries;
    for(uint8_t n = 0; n < lbrd->num_entries; n++){
        prev_lbrd->best_ids[n] = lbrd->best_ids[n]; 
        prev_lbrd->best_times[n] = lbrd->best_times[n]; 
    }
    for(uint8_t n = 0; n < 2*MAX_PLAYERS; n++)
        prev_lbrd->player_stats[n] = lbrd->player_stats[n];
    
    prev_lbrd->vld = true;
}

/*
* Precondition: lbrd->best_times (matching with lbrd->best_ids) sorted descendingly 
* Postcondition: lbrd->best_times (matching with lbrd->best_ids) sorted descendingly
                 lrbd->num_entries and lbrd->player_stats will be updated 
                 will staggered back up of leader-board to prev_lbrd
*/
void updateLeader(volatile leaderbrd * lbrd, volatile leaderbrd * prev_lbrd,\
                  uint8_t pnum, time_t game_time, bool won, bool force_sort){
    
    // for error detection to work at all make sure that upon
    // entering this function we check the vld states of the 
    // two copies of the leader board
    
    if(lbrd->vld == false && prev_lbrd->vld == true)
        restorePrevLeader(lbrd, prev_lbrd);
    else if(lbrd->vld == true && prev_lbrd->vld == false)
        restorePrevLeader(prev_lbrd, lbrd);
    else if(lbrd->vld == false && prev_lbrd->vld == false){
        perror("both copies of leaderboard are invalid!");
        if(kill(getpid(), SIGINT)) exit(EXIT_FAILURE);
        return;
    }

    lbrd->vld = false; //if thread dies before end, won't be set back to true

    lbrd->player_stats[2*pnum + 1]++;
    if(won){
        lbrd->player_stats[2*pnum]++;
    }
    else{
        lbrd->vld = true;
        prev_lbrd->vld = false; //if thread dies before end, won't be set back to true
        prev_lbrd->player_stats[2*pnum + 1]++;
        prev_lbrd->vld = true;
        return;   
    } 

    if(lbrd->num_entries < MAX_LRDBRD_ENTRIES){ 
        lbrd->num_entries++;
        lbrd->best_times[lbrd->num_entries - 1] = game_time;
        lbrd->best_ids[lbrd->num_entries - 1] = pnum;
    }
    else if(game_time < lbrd->best_times[0]){
        lbrd->best_times[0] = game_time;
        lbrd->best_ids[0] = pnum;
    }
    else if(game_time == lbrd->best_times[0]){
        uint8_t l_id = lbrd->best_ids[0];
        if(l_id != pnum){
            if(lbrd->player_stats[2*pnum] > lbrd->player_stats[2*l_id]){
                 lbrd->best_times[0] = game_time;
                 lbrd->best_ids[0] = pnum;
            }
            else if(lbrd->player_stats[2*pnum] == lbrd->player_stats[2*l_id]){
                unsigned char player_old[MAX_AUTH_FIELD_LEN];
                unsigned char player_new[MAX_AUTH_FIELD_LEN];
                uint8_t old_n = sprintf(player_old, "%s", lbrd->player_names[l_id]);
                uint8_t new_n = sprintf(player_new, "%s", lbrd->player_names[pnum]);
                toLower(player_old, old_n); toLower(player_new, new_n);
                if(wordBeforeWord(player_new, player_old, new_n, old_n)){
                    lbrd->best_times[0] = game_time;
                    lbrd->best_ids[0] = pnum;
                }
            }
        }
    }
    else if(!force_sort){
        lbrd->vld = true;
        prev_lbrd->vld = false; //if thread dies before end, won't be set back to true
        prev_lbrd->player_stats[2*pnum + 1]++;
        prev_lbrd->player_stats[2*pnum]++;
        prev_lbrd->vld = true;
        return;   
    } 

    // initial first round: modified Insertion sort 
    // descending, modified to detect duplicates
    // why insertion Sort: stable, in-place, and small input already mostly sorted
    uint8_t num_dup = 0;
    uint8_t tmp_id;
    time_t tmp_time;
    int8_t j;
    for(int8_t i = 1; i < lbrd->num_entries; i++){
        tmp_time = lbrd->best_times[i];
        tmp_id = lbrd->best_ids[i];
        j = i;
        while(j > 0 && lbrd->best_times[j - 1] <= tmp_time){ // ( DESCENDING < )  ( ASCENDING > )
            if(lbrd->best_times[j - 1] == tmp_time) num_dup++; // duplicates?
            // safe to mirror swaps in other array as insertion sort stable
            lbrd->best_times[j] = lbrd->best_times[j - 1];
            lbrd->best_ids[j] = lbrd->best_ids[j - 1]; 
            j--;  
        }
        // safe to mirror swaps in other array as insertion sort stable
        lbrd->best_times[j] = tmp_time;
        lbrd->best_ids[j] = tmp_id;    
    }
    if(num_dup == 0){
        lbrd->vld = true;
        backupLeader(lbrd, prev_lbrd);
        return;
    } 
    // second pass if duplicates exist
    uint8_t n = 0;
    for(uint8_t d = 0; d < num_dup; d++){
        for( ; n < lbrd->num_entries - 1; n++){
            if(lbrd->best_times[n] == lbrd->best_times[n + 1]){ // looking forward
                uint8_t until = n;
                while(lbrd->best_times[until] == lbrd->best_times[until + 1] \
                      && until != lbrd->num_entries - 1) until++;
                // sort subsection from until back to n by num_won ascending
                uint8_t num_dup_2 = 0; 
                uint16_t tmp_num_won;
                for(int8_t i = 1; i < until - n + 1; i++){
                    j = n + i;
                    tmp_id = lbrd->best_ids[j];
                    tmp_num_won = lbrd->player_stats[2*tmp_id];
                    while(j > n && lbrd->player_stats[2*lbrd->best_ids[j - 1]] >= tmp_num_won){
                        if(lbrd->player_stats[2*lbrd->best_ids[j - 1]] == \
                           tmp_num_won) num_dup_2++;

                        lbrd->best_ids[j] = lbrd->best_ids[j - 1];          
                        j--;
                    }
                    lbrd->best_ids[j] = tmp_id;               
                }
                if(num_dup_2 == 0){
                    n += until;
                    break;
                }
                // now sorting subsubregions by name (alphabetic order) ascending
                uint8_t n2 = n;
                for(uint8_t d2 = 0; d2 < num_dup_2; d2++){
                    for(; n2 < until; n2++){
                        if(lbrd->player_stats[2*lbrd->best_ids[n2]] == \
                           lbrd->player_stats[2*lbrd->best_ids[n2 + 1]]){
                            uint8_t until2 = n2;
                            while(lbrd->player_stats[2*lbrd->best_ids[until2]] == \
                                  lbrd->player_stats[2*lbrd->best_ids[until2 + 1]] \
                                  && until2 != until) until2++;
                            unsigned char tmp_name[MAX_AUTH_FIELD_LEN]; uint8_t tmp_n;
                            unsigned char cmp_name[MAX_AUTH_FIELD_LEN]; uint8_t cmp_n;
                            for(int8_t i = 1; i < until2 - n2 + 1; i++){
                                j = n2 + i;
                                tmp_id = lbrd->best_ids[j];
                                tmp_n = sprintf(tmp_name, "%s", \
                                                lbrd->player_names[tmp_id]);
                                toLower(tmp_name, tmp_n);
                                cmp_n = sprintf(cmp_name, "%s", \
                                                lbrd->player_names[lbrd->best_ids[j - 1]]);
                                toLower(cmp_name, cmp_n);
                                while(j > n2 && (wordBeforeWord(tmp_name, cmp_name, tmp_n, cmp_n))){
                                    lbrd->best_ids[j] = lbrd->best_ids[j - 1];
                                    j--;
                                    cmp_n = sprintf(cmp_name, "%s", \
                                                    lbrd->player_names[lbrd->best_ids[j - 1]]);
                                    toLower(cmp_name, cmp_n);
                                }
                                lbrd->best_ids[j] = tmp_id;      
                            }
                            n2 += until2;
                        }
                    }
                }
            }
        }
    }
    lbrd->vld = true;
    backupLeader(lbrd, prev_lbrd);
}

#define TOTAL_WIDTH 90

void printLeader(unsigned char * curr_lbrd){

    uint16_t c = 0;
    uint8_t num_entries = 0;
    unsigned char num_entries_s[NUM_ENTRIES_CHARS]; // number of entries as a string
    do{
        num_entries_s[c] = curr_lbrd[c];
    }while(curr_lbrd[++c] != SEP);

    num_entries = atoi(num_entries_s);
    if(num_entries > MAX_LRDBRD_ENTRIES){
        fprintf(stderr, "too many leaderboard entries: %d", num_entries);
        exit(EXIT_FAILURE);
    }

    for(uint8_t l = 0; l < TOTAL_WIDTH; l++) printf("=");
    printf("\n");

    uint8_t i = 0;
    for(uint8_t n = 0; n < num_entries; n++){
        unsigned char name[MAX_AUTH_FIELD_LEN]; 
        time_t t = 0; unsigned char t_s[NUM_LU_CHARS];
        uint16_t num_won = 0; unsigned char num_won_s[NUM_U16_CHARS];
        uint16_t num_played = 0; unsigned char num_played_s[NUM_U16_CHARS];
        // name
        i = 0;        
        while(curr_lbrd[++c] != SEP) name[i++] = curr_lbrd[c];
        // time
        i = 0;
        while(curr_lbrd[++c] != SEP) t_s[i++] = curr_lbrd[c];
        t = atoi(t_s);
        // num_won
        i = 0;
        while(curr_lbrd[++c] != SEP) num_won_s[i++] = curr_lbrd[c];
        num_won = atoi(num_won_s);
        // num_played
        i  = 0;
        while(curr_lbrd[++c] != SEP) num_played_s[i++] = curr_lbrd[c];
        num_played = atoi(num_played_s);

        printf("%-10s%10lu seconds %10d games won %10d games played\n",\
            name,t,num_won,num_played);
    }
    for(uint8_t l = 0; l < TOTAL_WIDTH; l++) printf("=");
    printf("\n");
}

void sendLeader(int fd, uint8_t pnum){ // test this guy while we're at it

    uint16_t leader_size = 0;
    uint8_t num_entries = lbrd->num_entries; 
    unsigned char curr_lbrd[MAX_PACKET_LEN];

    uint8_t l = snprintf(NULL, 0, "%d", num_entries);
    unsigned char * s = malloc(l + 1);
    snprintf(s, l + 1, "%d", num_entries);
    for(uint8_t m = 0; m < l + 1; m++) 
        curr_lbrd[leader_size++] = s[m];
    free(s);
    curr_lbrd[leader_size++] = SEP;
    
    for(uint8_t n = 0; n < lbrd->num_entries; n++){
        
        uint8_t id = lbrd->best_ids[n];
        time_t best = lbrd->best_times[n];

        unsigned char * name = lbrd->player_names[id];

        uint32_t num_won = lbrd->player_stats[2*id];
        uint32_t num_played = lbrd->player_stats[2*id + 1];

        uint8_t temp[2] = {num_won, num_played};

        uint8_t i = 0;
        do{
            curr_lbrd[leader_size++] = name[i];
        }while(name[i++] != 0x00);
        curr_lbrd[leader_size++] = SEP;

        uint8_t l = snprintf(NULL, 0, "%lu", best);
        unsigned char * s = malloc(l + 1);
        snprintf(s, l + 1, "%lu", best);
        for(uint8_t m = 0; m < l + 1; m++) 
            curr_lbrd[leader_size++] = s[m];
        free(s);
        curr_lbrd[leader_size++] = SEP;

        for(uint8_t t = 0; t < 2; t++){
            uint8_t l = snprintf(NULL, 0, "%d", temp[t]);
            unsigned char * s = malloc(l + 1);
            snprintf(s, l + 1, "%d", temp[t]);
            for(uint8_t m = 0; m < l + 1; m++) 
                curr_lbrd[leader_size++] = s[m];
            free(s);
            curr_lbrd[leader_size++] = SEP;
        }
    }
    //sendPacket(fd, pnum, 'L', curr_lbrd, leader_size);
    printLeader(curr_lbrd);

    //for(uint16_t i = 0; i < leader_size; i++) printf("%c", curr_lbrd[i]);
    //printf("\n");
}

int main(int argc, char ** argv){

	if(!(lbrd = malloc(sizeof(*lbrd)))) 
        perror("failed to allocate leader board");

	initLeader(lbrd, false, prev_lbrd, NULL);

    if(!(prev_lbrd = malloc(sizeof(*prev_lbrd)))) 
        perror("failed to allocate leader board");

    initLeader(prev_lbrd, false, prev_lbrd, NULL);

	lbrd->num_entries = MAX_LRDBRD_ENTRIES;
	//lbrd->player_names[0] = "Maolin";
	//lbrd->player_names[1] = "Jason";
	//lbrd->player_names[2] = "Mike";
	//lbrd->player_names[3] = "Peter";
	//lbrd->player_names[4] = "Justin";
	//lbrd->player_names[5] = "Anna";
	//lbrd->player_names[6] = "Timothy";
	//lbrd->player_names[7] = "Anthony";
	//lbrd->player_names[8] = "Paul";
	//lbrd->player_names[9] = "Richie";

	lbrd->best_ids[0] = 0;
	lbrd->best_ids[1] = 2;
	lbrd->best_ids[2] = 7;
	lbrd->best_ids[3] = 5;
	lbrd->best_ids[4] = 9;
	lbrd->best_ids[5] = 1;
	lbrd->best_ids[6] = 8;
	lbrd->best_ids[7] = 2;
	lbrd->best_ids[8] = 6;
	lbrd->best_ids[9] = 1;
	lbrd->best_ids[10] = 8;
	lbrd->best_ids[11] = 4;
	lbrd->best_ids[12] = 5;
	lbrd->best_ids[13] = 0;
	lbrd->best_ids[14] = 4;
	lbrd->best_ids[15] = 3;
	lbrd->best_ids[16] = 7;
	lbrd->best_ids[17] = 9;
	lbrd->best_ids[18] = 6;
	lbrd->best_ids[19] = 3;
	lbrd->best_ids[20] = 1;	

	lbrd->best_times[0] = 7;
	lbrd->best_times[1] = 19;
	lbrd->best_times[2] = 24;
	lbrd->best_times[3] = 26;
	lbrd->best_times[4] = 28;
	lbrd->best_times[5] = 29;
	lbrd->best_times[6] = 29;
	lbrd->best_times[7] = 29;
	lbrd->best_times[8] = 30;
	lbrd->best_times[9] = 32;
	lbrd->best_times[10] = 33;
	lbrd->best_times[11] = 33;
	lbrd->best_times[12] = 34;
	lbrd->best_times[13] = 36;
	lbrd->best_times[14] = 38;
	lbrd->best_times[15] = 40;
	lbrd->best_times[16] = 51;
	lbrd->best_times[17] = 60;
	lbrd->best_times[18] = 104;
	lbrd->best_times[19] = 152;
	lbrd->best_times[20] = 1421;	

	lbrd->player_stats[0] = 2;      lbrd->player_stats[1] = 2; 
	lbrd->player_stats[2] = 3;      lbrd->player_stats[3] = 5; 
	lbrd->player_stats[4] = 2;      lbrd->player_stats[5] = 2; 
	lbrd->player_stats[6] = 2;      lbrd->player_stats[7] = 2; 
	lbrd->player_stats[8] = 2;      lbrd->player_stats[9] = 4; 
	lbrd->player_stats[10] = 2;      lbrd->player_stats[11] = 3; 
	lbrd->player_stats[12] = 2;      lbrd->player_stats[13] = 3; 
	lbrd->player_stats[14] = 2;      lbrd->player_stats[15] = 2; 
	lbrd->player_stats[16] = 2;      lbrd->player_stats[17] = 5; 
	lbrd->player_stats[18] = 2;      lbrd->player_stats[19] = 2; 
	
    lbrd->vld = true;

    if(lbrd->vld == true) printf("T1\n");
    else printf("F1\n");

    if(prev_lbrd->vld == true) printf("T2\n");
    else printf("F2\n");

    fflush(stdout);

	updateLeader(lbrd, prev_lbrd, 0, time(NULL)-8, true, true);

	//updateLeader(0, time(NULL)-8, true, true);

	//updateLeader(6, time(NULL)-104, true);

	//updateLeader(9, time(NULL)-104, true);

	//printLeader();
    sendLeader(0, 0);

	destroyLeader(lbrd);
    destroyLeader(prev_lbrd);

	free((void*)lbrd);
    free((void*)prev_lbrd);

	return 0;
}