#include "game_client.h"

static volatile prev_packet * prev_pckt;
static volatile sig_atomic_t sigint_recv = 0;
static int sockfd;

int main(int argc, char ** argv){

    if (argc != 3) {
        fprintf(stderr,"usage: %s hostname port\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = handleSIGINT;
    if(sigaction(SIGINT, &act, NULL)){
        perror("sigaction");
        return EXIT_FAILURE;
    }

    uint8_t id; // obtained from server upon login

    if(!(prev_pckt = malloc(sizeof(*prev_pckt))))
        perror("failed to allocate prev_pckt");
    else initPrevPckt(prev_pckt);
    
    userLogIn(&sockfd, &id, argv);

	/* Interact with the user */

    char usel = '0'; // users choice from main menu
    
    unsigned char packet[MAX_PACKET_LEN];

    bool quit = false;
    while(!quit && (sigint_recv == 0)){

    	usel = welcome();
        
    	switch(usel){
    	    case '1': // play
                // send notice to server, get confirmation
                adviseMove(sockfd, id, 'S', 0, 0, packet, NULL);
                if(packet[PCKT_VERB] != 'S') 
                    printf("Server did not acknowledge. Please resend.\n");
                else{
                    gameController(sockfd, id);
                }
    	        break;
    	    case '2': // leaderboard
                adviseMove(sockfd, id, 'L', 0, 0, packet, NULL);
                if(packet[PCKT_VERB] != 'L')
                    printf("Server did not acknowledge. Please resend.\n");
                else{
                    printLeader(sockfd, &packet[PCKT_DATA]);
                    //printf("L: yay!\n"); fflush(stdout);
                }
    	        break;
    	    case '3': // quit
                sendPacket(sockfd, id, prev_pckt, 'Q', 0, 0);
                // don't worry about whether the server gets the message
    	    	quit = true;
    	        break;  
    	    default: // tidy
                perror("what?");        
    	}	
    }

    shutdownClient(sockfd, NULL);
}

static void handleSIGINT(int sig){
    // this ensures server will be notified of
    // client shutdown if Cntl-C is pressed
    sigint_recv = 1;
}

void shutdownClient(int fd, unsigned char ** part_map){
    sendChirp(fd, '*', NULL);
    if(part_map != NULL){
        for(uint8_t j = 0; j < NUM_TILES_X; j++)
            free(part_map[j]);
        free(part_map);
    }
    free(prev_pckt->data);
    free((void*)prev_pckt);
    shutdown(fd, SHUT_RD);
    close(fd);
    exit(EXIT_SUCCESS);
}
/*
 * Client side game controller 
 * Responsible for: allocating, sending and freeing a master copy of clients partial mine map, 
 *                  checking if a flag placement is valid,
 *                  checking death or progress, 
 *                  facilitating client entry and exit into playable games,
 *                  ? tracking play time ? or should the client do that ? or both ? with arbitration 
 * */
void gameController(int sockfd, uint8_t id){
    
    bool alive = true;
    bool willing = true;
    bool able = true;

    unsigned char packet[MAX_PACKET_LEN];

    unsigned char ** part_map;

    if((part_map = malloc(NUM_TILES_X * sizeof(*part_map)))){
        for(uint8_t j = 0; j < NUM_TILES_X; j++){ 
            if((part_map[j] = malloc(NUM_TILES_Y * sizeof(**part_map))) == NULL)
                fprintf(stderr, "\nfailed to allocate map[%d]\n", j); 
            else for(uint8_t i = 0; i < NUM_TILES_Y; i++) part_map[j][i] = '_';
        }
    }
    else perror("failed to allocate map");

    unsigned char verb;
    uint8_t x, y;
    uint8_t rem;

    /*
    *  Game event loops
    * */

    while(willing && able && sigint_recv == 0){
        
        rem = NUM_MINES;

        displayMap(part_map, rem);

        /*
        * Individual Game event loop
        * */
        while(alive && willing && able && rem > 0 && sigint_recv == 0){    
        
            verb = getPlayerMove(part_map, &x, &y); 

            uint8_t i, j, m;
            uint16_t tlen;
            unsigned char * time_s;
            switch(verb){
                case 'r':
                    verb = 'R';
                case 'R':
                    adviseMove(sockfd, id, verb, x, y, packet, part_map);
                    
                    // valid responses from server: M, D, C
                    switch(packet[PCKT_VERB]){
                        case 'M':
                            for(i = 0; i < NUM_TILES_X; i++) 
                                for(j = 0; j < NUM_TILES_Y; j++)
                                    part_map[j][i] = packet[PCKT_DATA + j*NUM_TILES_Y + i];
                            printf("\nWOOHOO! Lets unwrap that Blob!\n");
                            displayMap(part_map, rem);
                            break;
                        case 'C':
                            printf("%2x, %2x, %2x \n", packet[PCKT_DATA],packet[PCKT_DATA+1],packet[PCKT_DATA+2] + ASC_NUM);
                            part_map[packet[PCKT_DATA+1]][packet[PCKT_DATA]] \
                                = packet[PCKT_DATA+2];
                            displayMap(part_map, rem);
                            break;
                        case 'D':
                            tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                            tlen -= NUM_FRAME_BYTES - NUM_TILES;
                            time_s = malloc(tlen * sizeof(*time_s));
                            for(m = 0; m < tlen; m++) 
                                time_s[m] = packet[PCKT_DATA + NUM_TILES + m];

                            for(i = 0; i < NUM_TILES_X; i++) 
                                for(j = 0; j < NUM_TILES_Y; j++)
                                    part_map[j][i] = packet[PCKT_DATA + j*NUM_TILES_Y + i];

                            printf("\nYOU DONE DIED!\n\nTime played: %s seconds.\n", time_s);
                            free(time_s);
                            alive = false;
                            displayMap(part_map, rem);
                            break;
                        default:
                            printf("Server sent invalid verb.\n");
                            printf("Try replaying that move.\n");
                    }
                    break;
                case 'p':
                    verb = 'P';
                case 'P':
                    adviseMove(sockfd, id, verb, x, y, packet, part_map);
                    // valid responses W, Y, X
                    switch(packet[PCKT_VERB]){
                        case 'W':
                            tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                            tlen -= NUM_FRAME_BYTES - 2;
                            time_s = malloc(tlen * sizeof(*time_s));
                            for(m = 0; m < tlen; m++) 
                                time_s[m] = packet[PCKT_DATA + 2 + m];

                            if(--rem != 0){
                                perror("Server and client out of sync.\n");
                                rem = 0;
                            }

                            printf("\nWELL DONE SON! I KNEW YOU COULD DO IT!\n");
                            printf("\nYou won in %s seconds.\n", time_s);
                            free(time_s);
                            part_map[packet[PCKT_DATA+1]][packet[PCKT_DATA]] = '+';
                            displayMap(part_map, rem);
                            break;
                        case 'Y':
                            rem--;
                            part_map[packet[PCKT_DATA+1]][packet[PCKT_DATA]] = '+';
                            displayMap(part_map, rem);
                            break;
                        case 'X':
                            printf("No mine is at that location.\n");
                            break;
                        default:
                            printf("Server sent invalid verb.\n");
                            printf("Try replaying that move.\n");
                    }
                    break;
                case 'Q':
                    verb = 'q'; // quitting a minefield, not whole game 
                case 'q':
                    adviseMove(sockfd, id, verb, 0, 0, packet, part_map);
                    if(packet[PCKT_VERB] == 'q') willing = false;
                    else printf("Server failed to acknowledge. Please resend.\n");
                    break; 
                default:
                    printf("Something done fucked up!");
                    able = false;
                    break;
            }
            if(packet[PCKT_ID] != id) perror("id mismatch");
        }
        /* reset the mine-field */
        for(uint8_t j = 0; j < NUM_TILES_X; j++){
            for(uint8_t i = 0; i < NUM_TILES_Y; i++){
                part_map[j][i] = '_';
            }
        }

        if(sigint_recv == 0){
            printf("\nWould you like to play again? [y/n]");
            unsigned char * ans = malloc(2*sizeof(*ans)); // extra char for \01
            scanf("%s", ans);
            if(ans[0] == 'y' || ans[0] == 'Y'){
                alive = true;
                willing = true;
                able = false;
                adviseMove(sockfd, id, 'S', 0, 0, packet, part_map);
                if(packet[PCKT_VERB] != 'S') 
                    printf("Server did not acknowledge properly. Exiting game.\n");
                else able = true;
            }
            else willing = false;
            free(ans);
        }
    }
    for(uint8_t j = 0; j < NUM_TILES_X; j++)
        free(part_map[j]);
    free(part_map);
}

void adviseMove(int fd, uint8_t id, unsigned char verb, uint8_t x, uint8_t y,\
              unsigned char packet[MAX_PACKET_LEN], unsigned char ** part_map){

    static uint8_t cnt = 0;
    int8_t pc = 0;
    unsigned char data[2];
    switch(verb){
        case 'q':
        case 'Q':
        case 'S':
        case 'L':
            sendPacket(fd, id, prev_pckt, verb, 0, 0);
            break;
        default:
            data[0] = x; data[1] = y;
            sendPacket(fd, id, prev_pckt, verb, data, 2);
    }

    if((pc = getPacket(false, ISSERVER, fd, packet)) == CHIRP){
        switch(packet[CHIRP_NAME]){
            case '+':
                printf("Notice: Server requests resend. Resending.\n");
                if(sigint_recv == 0){
                    if(cnt++ < RESEND_LIM)
                        adviseMove(fd, id, verb, x, y, packet, part_map);
                    else{
                        perror("adviseMove resend limit exceeded");
                        shutdownClient(fd, part_map);
                    }
                }
                break;
            case '*':
                printf("Server shut down.\n");
                shutdownClient(fd, part_map);
            default:
                printf("server sent invalid name of chirp in reply.\n");
                shutdownClient(fd, part_map);
        }
    }

    switch(pc){
        case STD_PCKT:
            if(packet[PCKT_ID] != id) 
                // for dev debugging
                perror("ids do not match");
            break;
        case INVLD_PCKT:
            perror("invalid packet received");
            break;
        case CONN_ERR:
            perror("Connection error. Exiting. Try restarting");
            shutdownClient(fd, part_map);
            break;
        case CONN_OFF:
            printf("Server disconnected. Exiting. Try again later.\n");
            shutdownClient(fd, part_map);
            break;
        default:
            fprintf(stderr,"unknown: pc = %d\n", pc);
    }

    cnt = 0;
}

/*
 * Player moves are vetted for invalid answers in this function, except for placing flag on non-mined tile
 * */
unsigned char getPlayerMove(unsigned char ** part_map, uint8_t * x, uint8_t * y){
    bool RPQ = true; // valid option selected
    bool inside = true; // valid coordinates given
    bool already = false; // tile not already revealed or flagged
    unsigned char resp = '0';
    unsigned char * s = malloc(UIN_LEN);
    printf("\nChoose an option:\n");
    printf("<R> Reveal tile\n<P> Place flag\n<Q> Quit game\n");
    do{
        printf("\nOption (R,P,Q): ");
        scanf("%s", s); resp = s[0];
        if((RPQ = (resp == 'R' || resp == 'P' || resp == 'r' || resp == 'p'))){
            do{
                printf("\nEnter tile coordinates (A - I, 1 - 9): ");
                scanf("%s", s); 
                
                inside = ((s[0] >= 'a' && s[0] <= 'i') ||\
                          (s[0] >= 'A' && s[0] <= 'I')) &&\
                         (s[1] >= '1' && s[1] <= '9');

                if(!inside)
                    printf("\n%c%c Invalid. (Eg, B2 or A8 would be valid)\n", s[0], s[1]);
                else{
                    *y = s[1] - ASC_NUM - 1;
                    if(s[0] >= 'a' && s[0] <= 'i') *x = s[0] - ASC_LOW;
                    else *x = s[0] - ASC_UPP;

                    if((already = (part_map[*y][*x] != '_')))
                        printf("\nTile already revealed or flagged.\n");
                }
            }while((!inside || already) && sigint_recv == 0);
        }
        else if(!(RPQ |= (resp == 'Q' || resp == 'q'))){
            printf("INVALID SELECTION: %c\n\n",resp);
        }        
    }while(!RPQ && sigint_recv == 0);

    free(s);
    return resp;
}

void userLogIn(int * sockfd, uint8_t * id, char ** argv){
    // get name and pword
    char name[MAX_AUTH_FIELD_LEN];
    char * pword;
    uint8_t name_len = 0, pword_len = 0;
    printf("\n");
    for(uint8_t i = 0; i < TOTAL_WIDTH/2; i++) printf("=");
    printf("\n");
    printf("Welcome to the online Minesweeper gaming system\n");
    for(uint8_t i = 0; i < TOTAL_WIDTH/2; i++) printf("=");
    printf("\n");
    printf("\nYou are required to logon with your registered user name and password.\n");
    
    printf("Username: ");
    fgets(name, MAX_AUTH_FIELD_LEN, stdin); //TODO: change to getpass()

    pword = getpass("Password: ");
    //fgets(pword, MAX_AUTH_FIELD_LEN, stdin); //TODO: change to getpass()

    while(name[name_len] != '\0' && name[name_len] != '\n') name_len++;
    while(name[pword_len] != '\0' && name[pword_len] != '\n') pword_len++;
        
    
    unsigned char auth_data[2*MAX_AUTH_FIELD_LEN + 1];
    uint8_t r_data_len = 0;
    for(uint8_t l = 0; l < name_len; l++)
        if(humanReadable(name[l], false)) auth_data[r_data_len++] = name[l];
    auth_data[r_data_len++] = SEP;
    for(uint8_t l = 0; l < pword_len; l++)
        if(humanReadable(pword[l], false)) auth_data[r_data_len++] = pword[l];

    setupConnection(sockfd, argv);

    /* User log-in */
    unsigned char chirp[CHIRP_LEN];

    if(getPacket(true, ISSERVER, *sockfd, chirp) != CHIRP){
        printf("server sent incorrect (non-chirp) reply - 1.\n");
        exit(EXIT_FAILURE);
    }
    if(chirp[CHIRP_NAME] == '?') adviseAuth(*sockfd, chirp, auth_data, r_data_len);

    switch(chirp[CHIRP_NAME]){
        case 'A':
            *id = chirp[CHIRP_DATA];
            printf("Password accepted.\n");
            break;
        case 'D':
            printf("You entered either an incorrect username or password. Disconnecting.\n");
            shutdownClient(*sockfd, NULL);
        case '|':
            printf("An instance of this client is already logged in. Disconnecting.\n");
            shutdownClient(*sockfd, NULL);
        case '*':
            printf("Server shut down.\n");
            shutdownClient(*sockfd, NULL);
        default:
            printf("Server sent invalid name of chirp in reply.\n");
            shutdownClient(*sockfd, NULL);
    }
    // you either successfully logon, or exit with EXIT_FAILURE
}

void adviseAuth(int fd, unsigned char chirp[CHIRP_LEN], unsigned char auth_data[2*MAX_AUTH_FIELD_LEN + 1],\
              uint16_t r_data_len){
    
    chirp[CHIRP_NAME] = '0';
    uint8_t cnt = 0;
    int8_t pc = 0;
    bool resend = false;
    do{
        sendPacket(fd, 0, prev_pckt, 'A', auth_data, r_data_len);
        if((pc = getPacket(true, ISSERVER, fd, chirp)) != CHIRP){
            printf("Server sent incorrect (non-chirp) reply - 2.");
            switch(pc){
                case CONN_ERR:
                    perror("CONN_ERR");
                    break;
                case CONN_OFF:
                    perror("CONN_OFF");
                    break;
                case INVLD_PCKT:
                    perror("INVLD_PCKT");
                    break;
                case STD_PCKT:
                    perror("STD_PCKT");
                    break;
                default:
                    perror("Unknown");
            }
            shutdownClient(fd, NULL);
        }

        if((resend = (chirp[CHIRP_NAME] == '+'))) 
            printf("Notice: Server requests resend. Resending.\n");
    }while(resend && cnt++ < RESEND_LIM);
}

void setupConnection(int * sockfd, char ** argv){
    // setup the socket and connect
    int errcode;
    struct addrinfo server;
    struct addrinfo *result, *rp;
    memset(&server, 0, sizeof(struct addrinfo));

    server.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    server.ai_socktype = SOCK_STREAM; /* Datagram socket */
    server.ai_flags = 0;
    server.ai_protocol = 0;          /* Any protocol */

    if ((errcode = getaddrinfo(argv[1], argv[2], &server, &result)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
        shutdownClient(*sockfd, NULL);
    }
    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        *sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (*sockfd == NO_SOCKET) continue;
        if (connect(*sockfd, rp->ai_addr, rp->ai_addrlen) != RET_ERR) break; /* Success */
        shutdownClient(*sockfd, NULL);
    }    

    if (rp == NULL) { /* No address succeeded */
        perror("could not connect");
        shutdownClient(*sockfd, NULL);
    }

    freeaddrinfo(result); /* No longer needed */
}

void printLeader(int fd, unsigned char * curr_lbrd){

    uint16_t c = 0;
    uint8_t num_entries = 0;
    char num_entries_s[NUM_ENTRIES_CHARS]; // number of entries as a string
    do{
        num_entries_s[c] = curr_lbrd[c];
    }while(curr_lbrd[++c] != SEP);

    num_entries = atoi(num_entries_s);
    if(num_entries > MAX_LRDBRD_ENTRIES){
        fprintf(stderr, "\ntoo many leaderboard entries: %d\n", num_entries);
        shutdownClient(fd, NULL);
    }
    else if(num_entries == 0){
        printf("\nThere are no leaderboard entries yet.\n");
        return;
    }
    else if(num_entries < 0){
        perror("num_entries in leaderboard less than zero");
        return;
    }

    for(uint8_t l = 0; l < TOTAL_WIDTH; l++) printf("=");
    printf("\n");

    uint8_t i = 0;
    for(uint8_t n = 0; n < num_entries; n++){
        unsigned char name[MAX_AUTH_FIELD_LEN]; 
        time_t t = 0; char t_s[NUM_LU_CHARS];
        uint16_t num_won = 0; char num_won_s[NUM_U16_CHARS];
        uint16_t num_played = 0; char num_played_s[NUM_U16_CHARS];
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

void displayMap(unsigned char ** part_map, uint8_t rem){
    
    bool dead = false;
    printf("\n== Remaining mines: %d ==\n\n", rem);
    printf("    ");
    for(uint8_t i = 0; i < NUM_TILES_X; i++) printf("%d ", i+1);
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_X*2 + 3; i++) printf("-");
    printf("\n");
    for(uint8_t i = 0; i < NUM_TILES_Y; i++){
        printf("%c | ", ASC_UPP + i); // ascii decimal offset 65
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
        	if ((dead = (part_map[j][i] == '*'))) printf("%c ", part_map[j][i]);
            else if(part_map[j][i] != '_') printf("%c ", part_map[j][i]);
            else printf("  ");
        }
        printf("\n");
    }
    printf("\n");
}

unsigned char welcome(void){

    char * uin;
    char retval;

    if((uin = (char*)malloc(UIN_LEN * sizeof(*uin))) == NULL)
        fprintf(stderr, "failed to allocate char array for user input");
    printf("\nWelcome to the Minesweeper gaming system.\n\n");
    printf("Please enter a selection:\n");
    printf("<1> Play Minesweeper\n");
    printf("<2> Show Leaderboard\n");
    printf("<3> Quit\n\n");
    do{
        printf("Selection option (1-3): ");
        scanf("%s", uin);
        if(uin[0] != '1' && uin[0] != '2' && uin[0] != '3') 
            printf("invalid selection \n");
    }while(uin[0] != '1' && uin[0] != '2' && uin[0] != '3' && sigint_recv == 0);
    retval = uin[0];
    free(uin);
    return retval;
}