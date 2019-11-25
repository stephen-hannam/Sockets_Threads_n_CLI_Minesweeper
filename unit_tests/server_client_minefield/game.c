
/*
 *  INT8_MIN: mine
 *  0: flag on mine
 *  1 to 8: num mines adjacent to tile (a numbered tile) not immediately adjacent to contiguous region of zero tiles
 *  9 to INT8_MAX: ids for contiguous zero tiles
 *  -1 to -INT8_MAX: ids for numbered tiles that fringe a contiguous region of zero tiles
 * */

void test_displayMap(int8_t ** map){
    int rem = 0;
    
    printf("\n== Remaining mines: %d\n ==", rem);
    printf("    ");
    for(uint8_t i = 0; i < NUM_TILES_X; i++) printf("%d ", i);
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_X*2 + 3; i++) printf("-");
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_Y; i++){ printf("%c | ",65+i); // ascii decimal offset 65
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            if(map[j][i] == MINE) printf("* ");
            else if(map[j][i] == FLAG) printf("+ ");
            else if(map[j][i] > FLAG && map[j][i] < 9) printf("%d ", map[j][i]);
//            else if(map[j][i] < FLAG) printf("%d ", (-1*(map[j][i])) % 10);
            else if(map[j][i] < FLAG) printf("%1X ", map[j][i]);
            else if(map[j][i] > 8) printf("%1X ", map[j][i]);
            else printf("  ");
        }
        printf("\n"); 
    }
    printf("\n");
}

int main(int argc, char ** argv){

    char usel = '0'; // users choice from main menu
    /*
     * Allocate memory for the full map of data needed for a single game
     * */
    int8_t ** map; 
    if(map = (int8_t**)malloc(NUM_TILES_X * sizeof(*map))){
        for(uint8_t i = 0; i < NUM_TILES_X; i++){ 
            if(map[i] = (int8_t*)malloc(NUM_TILES_Y * sizeof(**map))){} //TODO
            else printf("\nfailed to allocate map\n"); //TODO
        }
    }
    else printf("\nfailed to allocate map\n");
    // player: single contiguous array holding details of players: name, # won, times of each game (also serves as # played) 
    // fields {16 bytes for name, 2 bytes for # won (MAX 65535), [1 byte times, 0x00, 2 byte times]} <- 0x00 separator
    // provide 2048 Bytes -> can store ~ 2048 - 18 game times
    //
    // Consider: wrapping everything in a single array (part_map + player) - include currenct x,y move in player array
    unsigned char * player = calloc(PLAYER_ARR_LEN, sizeof(*player));

    srand(RAND_NUM_SEED);

    usel = welcome();
    
    switch(usel){
        case '1': // play
            initMap(map);
            //test_displayMap(map);
            //initPlayer(player);
            gameController(map, player);
            break;
        case '2': // leaderboard
            break;
        case '3': // quit
            break;  
        default: // tidy
            break;          
    } 

    /* free dynamically allocated memory */
    free(map); 
    free(player);
    return 0;
}
