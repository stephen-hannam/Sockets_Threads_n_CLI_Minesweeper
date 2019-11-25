#include "client.h"

static volatile prev_packet * prev_pckt;

int main(int argc, char ** argv){

    if (argc != 3) {
        fprintf(stderr,"usage: %s hostname port\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint8_t id; // obtained from server upon login
	int sockfd;

    if(!(prev_pckt = malloc(sizeof(*prev_pckt))))
        perror("failed to allocate prev_pckt");
    else initPrevPckt(prev_pckt);
    
    userLogIn(&sockfd, &id, argv);

	/* Interact with the user */

    char usel = '0'; // users choice from main menu
     
    unsigned char packet[MAX_PACKET_LEN];

    bool quit = false;
    while(!quit){

    	usel = welcome();
        
    	switch(usel){
    	    case '1': // play
                // send notice to server, get confirmation
                adviseMove(sockfd, id, 'S', 0, 0, packet);
                if(packet[PCKT_VERB] != 'S') 
                    printf("Server did not acknowledge. Please resend manually.\n");
                else{
                    printf("S: yay!\n"); fflush(stdout);
                    gameController(NULL, sockfd, id);
                }
    	        break;
    	    case '2': // leaderboard
                adviseMove(sockfd, id, 'L', 0, 0, packet);
                if(packet[PCKT_VERB] != 'L')
                    printf("Server did not acknowledge. Please resend manually.\n");
                else{
                    printf("L: yay!\n"); fflush(stdout);
                }
    	        break;
    	    case '3': // quit
                sendPacket(sockfd, id, prev_pckt, 'Q', 0, 0);
                printf("sent Q\n"); fflush(stdout);
                // don't worry about whether the server gets the message
    	    	quit = true;
    	        break;  
    	    default: // tidy
                perror("Something done fucked up!\n");
                return EXIT_FAILURE;        
    	}	
    }

    shutdownClient(sockfd);
}

void shutdownClient(int fd){
    free(prev_pckt->data);
    free((void*)prev_pckt);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    exit(EXIT_SUCCESS);
}

/*
 * Server side game controller 
 * Responsible for: allocating, sending and freeing a master copy of clients partial mine map, 
 *                  checking if a flag placement is valid,
 *                  checking death or progress, 
 *                  facilitating client entry and exit into playable games,
 *                  ? tracking play time ? or should the client do that ? or both ? with arbitration 
 * */
void gameController(unsigned char ** part_map, int sockfd, uint8_t id){
    
    bool alive = true;
    bool willing = true;
    bool able = true;
    unsigned char verb;
    uint8_t x, y;

    unsigned char packet[MAX_PACKET_LEN];
    unsigned char chirp[CHIRP_LEN];

    /*
     * Game event loop
     * */
    // displayMap(part_map, rem); //TODO: send part_map to client

    while(alive && willing && able){    
        
        verb = getPlayerMove(NULL, &x, &y); 

        uint8_t i, j, m;
        uint16_t tlen;
        switch(verb){
            case 'r':
                verb = 'R';
            case 'R':
                adviseMove(sockfd, id, verb, x, y, packet);
                // valid responses from server: M, D, C
                switch(packet[PCKT_VERB]){
                    case 'M':
                        printf("M\n");
                        tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                        printf("quoted length: %d\n", tlen);
                        tlen -= NUM_FRAME_BYTES - NUM_TILES;
                        printf("data length: %d\n", tlen);
                        fflush(stdout);
                        break;
                    case 'C':
                        printf("C\n");
                        tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                        printf("quoted length: %d\n", tlen);
                        tlen = tlen - NUM_FRAME_BYTES;
                        printf("data length: %d\n", tlen);
                        fflush(stdout);
                        break;
                    case 'D':
                        printf("D\n"); 
                        tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                        printf("quoted length: %d\n", tlen);
                        tlen -= NUM_FRAME_BYTES - NUM_TILES;
                        printf("data length: %d\n", tlen);
                        fflush(stdout);
                        alive = false;
                        break;
                    default:
                        printf("Server sent invalid verb.\n");
                        printf("Try replaying that move.\n");
                }

                break;
            case 'p':
                verb = 'P';
            case 'P':
                adviseMove(sockfd, id, verb, x, y, packet);
                // valid responses W, Y, X
                switch(packet[PCKT_VERB]){
                    case 'W':
                        printf("W\n");
                        tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                        printf("tlen: %d\n", tlen); fflush(stdout);
                        tlen -= NUM_FRAME_BYTES + 2;
                        printf("tlen: %d\n", tlen); fflush(stdout);
                        fflush(stdout);
                        break;
                    case 'Y':
                        printf("Y\n");
                        tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                        printf("tlen: %d\n", tlen); fflush(stdout);
                        tlen -= NUM_FRAME_BYTES + 2;
                        printf("tlen: %d\n", tlen); fflush(stdout);
                        fflush(stdout);
                        break;
                    case 'X':
                        printf("X\n");
                        tlen = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8);
                        printf("tlen: %d\n", tlen); fflush(stdout);
                        tlen -= NUM_FRAME_BYTES + 2;
                        printf("tlen: %d\n", tlen); fflush(stdout);
                        fflush(stdout);
                        break;
                    default:
                        printf("Server sent invalid verb.\n");
                        printf("Try replaying that move.\n");
                }
                break;
            case 'Q':
                verb = 'q'; // quitting a minefield, not whole game 
            case 'q':
                adviseMove(sockfd, id, verb, 0, 0, packet);
                if(packet[PCKT_VERB] == 'q'){
                    printf("q\n");
                    fflush(stdout);
                    willing = false;    
                } 
                else printf("Server failed to acknowledge. Please resend manually.\n");
                break; 
            default:
                printf("Something done fucked up!");
                able = false;
                break;
        }
        if(packet[PCKT_ID] != id) perror("id mismatch");
    } 
}

void adviseMove(int fd, uint8_t id, unsigned char verb, uint8_t x, uint8_t y,\
              unsigned char packet[MAX_PACKET_LEN]){

    static uint8_t cnt = 0;
    int8_t pc = 0;
    unsigned char data[2];
    switch(verb){
        case 'q':
        case 'Q':
        case 'S':
        case 'L':
            printf("player %d sending %c on %d\n", id, verb, fd); fflush(stdout);
            sendPacket(fd, id, prev_pckt, verb, 0, 0);
            break;
        default:
            data[0] = x; data[1] = y;
            printf("player %d sending %c on %d\n", id, verb, fd); fflush(stdout);
            sendPacket(fd, id, prev_pckt, verb, data, 2);
    }

    if((pc = getPacket(false, ISSERVER, fd, packet)) == CHIRP){
        printf("chirp recvd\n"); fflush(stdout);
        switch(packet[CHIRP_NAME]){
            case '+':
                printf("Notice: Server requests resend. Resending.\n");
                if(cnt++ < RESEND_LIM)
                    adviseMove(fd, id, verb, x, y, packet);
                else{
                    perror("advise move resend limit exceeded");
                    shutdownClient(fd);
                }
                break;
            case '*':
                printf("Server shut down.\n");
                shutdownClient(fd);
            default:
                printf("server sent invalid name of chirp in reply.\n");
                shutdownClient(fd);
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
            shutdownClient(fd);
            break;
        case CONN_OFF:
            printf("Server disconnected. Exiting. Try again later.\n");
            shutdownClient(fd);
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
    uint8_t ascii_offset = ASC_UPP; // assumes use of capital letters
    printf("\nChoose an option:\n");
    printf("<R> Reveal tile\n<P> Place flag\n<Q> Quit game\n");
    do{
        printf("\nOption (R,P,Q): ");
        scanf("%s", s); resp = s[0];
        if(RPQ = (resp == 'R' || resp == 'P' || resp == 'r' || resp == 'p')){
            do{
                printf("\nEnter tile coordinates (A - I, 1 - 9): ");
                scanf("%s", s); if(s[0] >= 'a' && s[0] <= 'i') ascii_offset = ASC_LOW;
                *y = s[1] - ASC_NUM; *x = s[0] - ascii_offset;
                if(!(inside = (*x >= 0 && *x <= NUM_TILES_X - 1))) 
                    printf("\n%c%c Invalid. (Eg, B2 or A8 would be valid)\n", s[0], s[1]);
                else if(!(inside |= (*y >= 0 && *y <= NUM_TILES_Y - 1))) 
                    printf("\n%c%c Invalid. (Eg, B2 or A8 would be valid)\n", s[0], s[1]);
                //else if(already = (part_map[*y][*x] != '_')) 
                //    printf("\nTile already revealed or flagged.\n");
            }while(!inside || already);
        }
        else if(!(RPQ |= (resp == 'Q' || resp == 'q'))){
            printf("invalid selection\n\n");
        }        
    }while(!RPQ);
    return resp;
}

void userLogIn(int * sockfd, uint8_t * id, char ** argv){
    // get name and pword
    unsigned char name[MAX_AUTH_FIELD_LEN];
    unsigned char * pword;
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

    printf("%d, %d\n",name_len, pword_len);

    /* debug*/
    for(int i = 0; i < name_len; i++){
        printf("%c", name[i]);
    }
    printf("\n");
    for(int i = 0; i < pword_len; i++){
        printf("%c", pword[i]);
    }
    printf("\n");
    /* debug*/

    unsigned char auth_data[2*MAX_AUTH_FIELD_LEN + 1];
    uint8_t r_data_len = 0;
    for(uint8_t l = 0; l < name_len; l++)
        if(humanReadable(name[l], false)) auth_data[r_data_len++] = name[l];
    auth_data[r_data_len++] = SEP;
    for(uint8_t l = 0; l < pword_len; l++)
        if(humanReadable(pword[l], false)) auth_data[r_data_len++] = pword[l];


    /* debug*/
    for(int i = 0; i < r_data_len; i++){
        if(auth_data[i] != SEP) printf("%c",auth_data[i]);
        else printf("_");
    }
    printf("\n");
    /* debug*/

    setupConnection(sockfd, argv);

    /* User log-in */
    unsigned char chirp[CHIRP_LEN];

    if(getPacket(true, ISSERVER, *sockfd, chirp) != CHIRP){
        printf("server sent incorrect (non-chirp) reply - 1.\n");
        shutdownClient(*sockfd);
    }
    if(chirp[CHIRP_NAME] == '?') adviseAuth(*sockfd, chirp, auth_data, r_data_len);

    switch(chirp[CHIRP_NAME]){
        case 'A':
            *id = chirp[CHIRP_DATA];
            printf("Password accepted.\n");
            break;
        case 'D':
            printf("You entered either an incorrect username or password. Disconnecting.\n");
            shutdownClient(*sockfd);
        case '|':
            printf("An instance of this client is already logged in. Disconnecting.\n");
            shutdownClient(*sockfd);
        case '*':
            printf("Server shut down.\n");
            shutdownClient(*sockfd);
        default:
            printf("server sent invalid name of chirp in reply.\n");
            shutdownClient(*sockfd);
    }
    // you either successfully logon, or exit with EXIT_FAILURE
}

void adviseAuth(int fd, unsigned char chirp[CHIRP_LEN], unsigned char auth_data[2*MAX_AUTH_FIELD_LEN + 1],\
              uint16_t r_data_len){
    
    chirp[CHIRP_NAME] == '0';
    uint8_t cnt = 0;
    int8_t pc = 0;
    bool resend = false;
    do{
        sendPacket(fd, 0, prev_pckt, 'A', auth_data, r_data_len);
        if((pc = getPacket(true, ISSERVER, fd, chirp)) != CHIRP){
            printf("server sent incorrect (non-chirp) reply - 2.\n");
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
            shutdownClient(fd);
        }
        if(resend = (chirp[CHIRP_NAME] == '+'))
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
        shutdownClient(*sockfd);
    }
    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        *sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (*sockfd == NO_SOCKET) continue;
        if (connect(*sockfd, rp->ai_addr, rp->ai_addrlen) == CONN_SUCCESS) break; /* Success */
        shutdownClient(*sockfd);
    }    

    if (rp == NULL) { /* No address succeeded */
        perror("could not connect");
        shutdownClient(*sockfd);
    }

    freeaddrinfo(result); /* No longer needed */
}

unsigned char welcome(void){

    char * uin;
    char retval;

    if((uin = (char*)malloc(UIN_LEN * sizeof(*uin))) == NULL)
        fprintf(stderr, "failed to allocate char array for user input");
    printf("Welcome to the Minesweeper gaming system.\n\n");
    printf("Please enter a selection:\n");
    printf("<1> Play Minesweeper\n");
    printf("<2> Show Leaderboard\n");
    printf("<3> Quit\n\n");
    do{
        printf("Selection option (1-3): ");
        scanf("%s", uin);
        if(uin[0] != '1' && uin[0] != '2' && uin[0] != '3') 
            printf("invalid selection \n");
    }while(uin[0] != '1' && uin[0] != '2' && uin[0] != '3');
    retval = uin[0];
    free(uin);
    return retval;
}