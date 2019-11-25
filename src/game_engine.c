#include "game_engine.h"

static char const * const AUTH_FILE_NAME = "Authentication.txt";

/* ============================================================== */
/* ============ <from inside any server side thread> ============ */

/* ============ </from inside any server side thread> ============ */
/* =============================================================== */


/* ************************
* SETTING UP THE MINE-FIELD
**************************** */

void placeMines(int8_t ** map, struct drand48_data * randBuffer){

    for(uint8_t i = 0; i < NUM_MINES; i++){
        long int rx, ry;
        uint8_t x, y;
        do{
            lrand48_r(randBuffer, &rx);
            lrand48_r(randBuffer, &ry); 

            x = (uint8_t)(rx % NUM_TILES_X);
            y = (uint8_t)(ry % NUM_TILES_Y);
        }while(map[y][x] == MINE);
        map[y][x] = MINE;
    }

}

/*
 * Precondition: place_mines() called for* map[][], only MINE and 0 are values in map[][]
 * Postcondition: map[][] values updated with adjacency values
 * */
void placeAdjs(int8_t ** map){
    // 3x3 window
    /* scan all non-boundary cells of* map */
    uint8_t xi, yi, xf, yf; 
    for(uint8_t i = 0; i < NUM_TILES_X; i++){
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map[j][i] == MINE) continue;
            if(i == 0) {xi = i; xf = i + 1;} 
            else if(i == NUM_TILES_X - 1) {xi = i - 1; xf = i;} 
            else {xi = i - 1; xf = i + 1;} 
            if(j == 0) {yi = j; yf = j + 1;} 
            else if(j == NUM_TILES_Y - 1) {yi = j - 1; yf = j;} 
            else {yi = j - 1; yf = j + 1;} 
            for(uint8_t ii = xi; ii <= xf; ii++){
                for(uint8_t jj = yi; jj <= yf; jj++){
                    if(map[jj][ii] == MINE) map[j][i]++;
                }   
            } 
        }
    }
}

void blobRelabel(int8_t ** map, int8_t old, int8_t new){
    for(uint8_t i = 0; i < NUM_TILES_X; i++){
        for(uint8_t j = 0; j < NUM_TILES_Y; j++) if(map[j][i] == old) map[j][i] = new;    
    } 
}

void newLabel(int8_t ** map, int8_t * tiles, uint8_t count){
    // find min in neighbor values
    int8_t min = tiles[0];
    for(uint8_t i = 0; i < count; i++){
        for(uint8_t j = 0; j < count; j++){
            if(tiles[i] < tiles[j]){
                min = tiles[i];
            }
        }    
    }
    // replace all non-min values in tiles with the min value
    for(uint8_t i = 0; i < count; i++){
        if(tiles[i] != min) blobRelabel(map, tiles[i], min);
    }
}

/*
 * Descr: modifies values of non-zero adjacency squares to identify them as bordering a given
 *        region of contiguous zero tiles, allowing look-up based expansion of tile regions
 * */
void addBlobFringes(int8_t ** map){
    uint8_t xi, yi, xf, yf;
    for(uint8_t i = 0; i < NUM_TILES_X; i++){
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map[j][i] < LBL_OFFSET) continue;
            if(i == 0) {xi = i; xf = i + 1;} 
            else if(i == NUM_TILES_X - 1) {xi = i - 1; xf = i;} 
            else {xi = i - 1; xf = i + 1;} 
            if(j == 0) {yi = j; yf = j + 1;} 
            else if(j == NUM_TILES_Y - 1) {yi = j - 1; yf = j;} 
            else {yi = j - 1; yf = j + 1;} 
            for(uint8_t ii = xi; ii <= xf; ii++){
                for(uint8_t jj = yi; jj <= yf; jj++){
                    if(map[jj][ii] < LBL_OFFSET && map[jj][ii] > 0)
                        map[jj][ii] = -1*((map[j][i] - LBL_OFFSET)*10 + map[jj][ii]);
                }   
            } 
        }
    } 
}

/*
 * Descr: A simple Connected-Component Labelling implementation to label contiguous
 *        regions of tiles with no adjacent mines, for easy region expansion during
 *        game play. Uses the standard two-pass algorithm.
 *        Additionally, add final label value to non-zero tiles, so they are also
 *        automatically unveiled by a bit of simple arithmetic, use addBlobFringes()
 * Precondition: place_mines() and place_adjs() called for this map[][]
 *               0 vals in map[][] still indicate tiles with no adjacent mines
 * Postcondition: all contiguous zero tiles given unique id numbers in map[][]
 *                map[][] should contain no 0 values
 * */
void placeBlobs(int8_t ** map){
    uint8_t currid = LBL_OFFSET - 1;
    bool neighbors = false;
    uint8_t uniq[10] = {0,0,0,0,0,0,0,0,0,0};
    // 1st pass -- label by 3x3 neighbors or incrementing currid
    uint8_t xi, yi, xf, yf;
    for(uint8_t i = 0; i < NUM_TILES_X; i++){
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map[j][i] != BLOB) continue;
            if(i == 0) {xi = i; xf = i + 1;} 
            else if(i == NUM_TILES_X - 1) {xi = i - 1; xf = i;} 
            else {xi = i - 1; xf = i + 1;} 
            if(j == 0) {yi = j; yf = j + 1;} 
            else if(j == NUM_TILES_Y - 1) {yi = j - 1; yf = j;} 
            else {yi = j - 1; yf = j + 1;} 
            for(uint8_t ii = xi; ii <= xf; ii++){
                for(uint8_t jj = yi; jj <= yf; jj++){
                    if(map[jj][ii] >= LBL_OFFSET){
                        map[j][i] = map[jj][ii];
                        neighbors = true;
                        break;
                    }
                }
                if(neighbors) break;   
            }
            if(!neighbors) map[j][i] = ++currid;
            neighbors = false;
        }
    }
    // 2nd pass -- exclude corners
    uint8_t count;
    for(uint8_t i = 0; i < NUM_TILES_X; i++){
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            // get a slight performance increase by stepping out 
            // (continuing) sooner than if all conditions were 
            // checked in a single statement every time
            if(map[j][i] < LBL_OFFSET) continue;
            if(i == 0 && j == 0) continue; // corner
            if(i == 0 && j == NUM_TILES_Y - 1) continue; // corner
            if(i == NUM_TILES_X - 1 && j == 0) continue; // corner
            if(i == NUM_TILES_X - 1 && j == NUM_TILES_Y - 1) continue; // corner 
            int8_t tiles[9];
            count = 1;
            tiles[0] = map[j][i];
            if(i == 0) {xi = i; xf = i + 1;} 
            else if(i == NUM_TILES_X - 1) {xi = i - 1; xf = i;} 
            else {xi = i - 1; xf = i + 1;} 
            if(j == 0) {yi = j; yf = j + 1;} 
            else if(j == NUM_TILES_Y - 1) {yi = j - 1; yf = j;} 
            else {yi = j - 1; yf = j + 1;}
            for(uint8_t ii = xi; ii <= xf; ii++){
                for(uint8_t jj = yi; jj <= yf; jj++){
                    if(!(ii - i == 0 && jj - j == 0) && map[jj][ii] \
                        >= LBL_OFFSET && map[j][i] != map[jj][ii])
                        tiles[count++] = map[jj][ii];                
                }   
            } 
            if(count > 0) newLabel(map, tiles, count);
            else break;        
        }
    } 
    // adjust cnr cells, no need for relabelling
    int8_t cnrs[4][4] = {{0, 0, 1, 1},
                         {0, NUM_TILES_Y - 1, 1, -1},
                         {NUM_TILES_X - 1, 0, -1, 1},
                         {NUM_TILES_X - 1, NUM_TILES_Y - 1, -1, -1}};
    int8_t xc, yc; uint8_t x, y;
    for(uint8_t n = 0; n < 4; n++){
        xc = cnrs[n][0]; yc = cnrs[n][1];
        if(map[yc][xc] < LBL_OFFSET) continue;
        int8_t tiles[3];
        count = 0;
        for(int8_t i = 0; i < 2; i++){
            for(int8_t j = 0; j < 2; j++){
                x = xc + cnrs[n][2]*j;
                y = yc + cnrs[n][3]*i;
                if(map[y][x] >= LBL_OFFSET && !(x == xc && y == yc)){
                    tiles[count++] = map[y][x];
                }
            }
        }
        int8_t min = map[yc][xc];
        for(uint8_t i = 0; i < count; i++){
            for(uint8_t j = 0; j < count; j++){
                if(tiles[i] < tiles[j] && tiles[i] < min){
                    min = tiles[i];
                }
            }
        }
        // use minimum value label on corner
        if(min < map[yc][xc]) map[yc][xc] = min; 
    }
    /* tighten up the labels, force them to be: 9, 10, A, ... */
    // get the set of labels, uniq
    count = 0;
    uint8_t lbl = 0;
    for(uint8_t i = 0; i < NUM_TILES_X; i++){
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map[j][i] > LBL_OFFSET){
                lbl = map[j][i];
                for(uint8_t j = 0; j < 10; j++){
                    if(lbl == uniq[j]){
                        lbl = 0;
                        break;
                    } 
                }
            }
            if(lbl > LBL_OFFSET) uniq[count++] = lbl;
            lbl = 0;
        }
    }
    // sort ascending, INSERTION sort, 
    // as it is in-place and inputs aren't expected to be large
    uint8_t tmp;
    int8_t j;
    for(int8_t i = 1; i < count; i++){
        tmp = uniq[i];
        j = i - 1;
        while(j >= 0 && uniq[j] > tmp){
            uniq[j + 1] = uniq[j];
            j--;  
        }
        uniq[j + 1] = tmp;
    }
    // recreate and reassign new minimal value labels
    int8_t new = 10;
    for(int8_t i = 0; i < count; i++) 
        blobRelabel(map, uniq[i], new + i);
    // ensure non-zero tiles adjacent to zero are also appropriately revealed
    addBlobFringes(map);
}

/* *******************************
* ACCESSING or UPDATING MINE-FIELD 
*        and LEADERBOARD
********************************* */

/*
 * Precondition: id >= 9, map contains value id, tiles valued as id contiguous with all other tiles so valued
 * */
void revealBlob(int8_t ** map, unsigned char ** part_map, int8_t id){
    for(uint8_t i = 0; i < NUM_TILES_X; i++){
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map[j][i] == id) part_map[j][i] = '0';
            else if(map[j][i] < 0 && map[j][i] != MINE){
                if((-1*(map[j][i]))/10 == (id - LBL_OFFSET))
                    part_map[j][i] = ((-1*(map[j][i])) % 10) + ASC_NUM;
            }   
        }
    }
}

/*
 * retcodes (place_flag) :  0: no mine   1: eliminated a mine
 * retcodes (!place_flag): -1: dead      0: reveal blob,        1+: tile number
 *
 * Precondition: invalid player moves have already been handlded by some other routine before calling this one
 *               invalid player moves includes places a flag on a non-mined tile, or trying to reveal an already revealed tile
 * Postcondition: partial map to be sent to client to display in console will be current, death of player signalled
 *                if need be, or reduction in remaining mines signalled if need be
 * */
int8_t updateMap(int8_t ** map, unsigned char ** part_map, \
                 uint8_t x, uint8_t y, bool place_flag){    
    
    // PLACE
    if(place_flag){
        if(map[y][x] == MINE) return VLD; // success
        return INVLD; // invalid attempt to place flag on non-mine tile
    }

    // REVEAL
    if(map[y][x] == MINE){
        // populate part_map
        for(uint8_t i = 0; i < NUM_TILES_X; i++){
            for(uint8_t j = 0; j < NUM_TILES_Y; j++){
                if(map[j][i] == MINE || map[j][i] == FLAG) 
                    part_map[j][i] = '*';
                //else part_map[j][i] = '_'; // erase or don't erase other tiles ... ?
            }
        }
        return DEAD; // death
    }

    if(map[y][x] < LBL_OFFSET && map[y][x] > 0){
        part_map[y][x] = map[y][x] + ASC_NUM; // ASCII offset
        return part_map[y][x];     
    } 
    else if(map[y][x] < 0){
        part_map[y][x] = ((-1*(map[y][x])) % 10) + ASC_NUM;    
        return part_map[y][x];
    } 
    else if(map[y][x] >= LBL_OFFSET){
        revealBlob(map, part_map, map[y][x]); // zero region    
    } 
    return BLOB; // blob revealed
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
    if(!(best_ids = calloc(MAX_LRDBRD_ENTRIES, sizeof(*best_ids)))){
        perror("failed to allocate best_ids");
        nonrecover = true;
    }
    
    time_t * best_times;
    if(!(best_times = calloc(MAX_LRDBRD_ENTRIES, sizeof(*best_times)))){
        perror("failed to allocate best_times");
        nonrecover = true;   
    }

    uint16_t * player_stats;
    if(!(player_stats = calloc(2 * MAX_PLAYERS, sizeof(*player_stats)))){
        perror("failed to allocate player_stats");
        nonrecover = true;      
    }

    unsigned char ** names;
    if((names = calloc(MAX_PLAYERS, sizeof(*names)))){
        for(uint8_t n = 0; n < MAX_PLAYERS; n++){
            if((names[n] = calloc(MAX_AUTH_FIELD_LEN, sizeof(**names))) == NULL){
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
        perror("both copies of leaderboard are invalid");
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
                char player_old[MAX_AUTH_FIELD_LEN];
                char player_new[MAX_AUTH_FIELD_LEN];
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
                            char tmp_name[MAX_AUTH_FIELD_LEN]; uint8_t tmp_n;
                            char cmp_name[MAX_AUTH_FIELD_LEN]; uint8_t cmp_n;
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