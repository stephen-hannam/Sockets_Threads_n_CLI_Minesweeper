#include "game.h"
/*
 *  INT8_MIN: mine
 *  0: flag on mine
 *  1 to 8: num mines adjacent to tile (a numbered tile) not immediately adjacent to contiguous region of zero tiles
 *  9 to INT8_MAX: ids for contiguous zero tiles
 *  -1 to -INT8_MAX: ids for numbered tiles that fringe a contiguous region of zero tiles
 * */

int main(int argc, char ** argv){

    struct drand48_data * randBuffer;
    if((randBuffer = malloc(sizeof(*randBuffer))) == NULL){
        fprintf(stderr,"failed to allocate randBuffer\n");
    }
    //char usel = '0'; // users choice from main menu
    /*
     * Allocate memory for the full map of data needed for a single game
     * */
    int8_t ** map_raw;
    if((map_raw = (int8_t**)malloc(NUM_TILES_X * sizeof(*map_raw)))){
        for(uint8_t i = 0; i < NUM_TILES_X; i++){
            if((map_raw[i] = (int8_t*)malloc(NUM_TILES_Y * sizeof(**map_raw)))){}
            else printf("\nfailed to allocate map_raw\n");
        }
    }
    else printf("\nfailed to allocate map_raw\n");

    unsigned char ** map_parsed;
    if((map_parsed = (unsigned char**)malloc(NUM_TILES_X * sizeof(*map_parsed)))){
        for(uint8_t i = 0; i < NUM_TILES_X; i++){
            if((map_parsed[i] = (unsigned char*)malloc(NUM_TILES_Y * sizeof(**map_parsed)))){}
            else printf("\nfailed to allocate map_parsed\n");
        }
    }
    else printf("\nfailed to allocate map_parsed\n");
    // player: single contiguous array holding details of players: name, # won, times of each game (also serves as # played)
    // fields {16 bytes for name, 2 bytes for # won (MAX 65535), [1 byte times, 0x00, 2 byte times]} <- 0x00 separator
    // provide 2048 Bytes -> can store ~ 2048 - 18 game times
    //
    // Consider: wrapping everything in a single array (part_map + player) - include currenct x,y move in player array
    //unsigned char * player = calloc(PLAYER_ARR_LEN, sizeof(*player));

    srand(RAND_NUM_SEED);

    initMap(map_raw, randBuffer);

    for (uint8_t i = 0; i < NUM_TILES_X; i++){
        for (uint8_t j = 0; j < NUM_TILES_Y; j++){
            updateMap(map_raw,map_parsed,i,j,false);
        }
    }

    test_displayMap(map_raw, map_parsed);

    /* free dynamically allocated memory */
    free(map_raw);
    //free(player);
    return 0;
}

void test_displayMap(int8_t ** map_raw, unsigned char ** map_parsed){

    printf("\nRaw values used by connect-component labelling \nto define blobs and features\n\n");
    printf("      ");
    for(uint8_t i = 0; i < NUM_TILES_X; i++) printf("%d   ", i);
    printf("   Mine = * = %d", MINE);
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_X*2 + 3; i++) printf("---");
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_Y; i++){ printf("%c | ",ASC_UPP+i); // ascii decimal offset 65
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map_raw[j][i] == MINE) printf("%3c ",'*');
            else if(map_raw[j][i] == FLAG) printf("+    ");
            else if(map_raw[j][i] > FLAG && map_raw[j][i] < LBL_OFFSET) printf("%3d ", map_raw[j][i]);
//            else if(map_raw[j][i] < FLAG) printf("%d ", (-1*(map_raw[j][i])) % 10);
            else if(map_raw[j][i] < FLAG) printf("%3d ", map_raw[j][i]);
            else if(map_raw[j][i] > LBL_OFFSET - 1) printf("%3d ", map_raw[j][i]);
            else printf("     ");
        }
        printf("\n");
    }
    printf("\n");
    printf("\n");
    printf("Parsed values after blobs and features are resolved\n\n");
    printf("      ");
    for(uint8_t i = 0; i < NUM_TILES_X; i++) printf("%d   ", i);
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_X*2 + 3; i++) printf("---");
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_Y; i++){ printf("%c | ",ASC_UPP+i); // ascii decimal offset 65
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map_parsed[j][i] == MINE) printf("%3c ",'*');
            else if(map_parsed[j][i] == FLAG) printf("+    ");
            else if(map_parsed[j][i] > FLAG && map_parsed[j][i] < LBL_OFFSET) printf("%3c ", map_parsed[j][i]);
//            else if(map_parsed[j][i] < FLAG) printf("%d ", (-1*(map_parsed[j][i])) % 10);
            else if(map_parsed[j][i] < FLAG) printf("%3c ", map_parsed[j][i]);
            else if(map_parsed[j][i] > LBL_OFFSET - 1) printf("%3c ", map_parsed[j][i]);
            else printf("     ");
        }
        printf("\n");
    }
    printf("\n");

}

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

    if(place_flag){
        if(map[y][x] == MINE) return 1; // success
        return INVLD; // invalid attempt to place flag on non-mine tile
    }

    if(!place_flag && map[y][x] == MINE){
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

void initMap(int8_t ** map, struct drand48_data * randBuffer){
    placeMines(map, randBuffer);
    placeAdjs(map);
    placeBlobs(map);
}

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
