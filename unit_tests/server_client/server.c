#include "server.h"

static volatile prev_packet * prev_pckts;
static volatile bool active_request[MAX_PLAYERS];

request_t * requests = NULL;     /* head of linked list of requests. */
request_t * last_request = NULL; /* pointer to last request.         */

/* Global var signal handling */
static volatile sig_atomic_t sigint_recv = 0;
static volatile bool sigint_thrds = false; // only modified by parent thread, no race conditions

static void handleSIGINT(int sig);

/* Threads */
pthread_mutexattr_t lmut_attr;
pthread_mutex_t leaderbrd_mutex;
pthread_mutex_t request_mutex   = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static volatile int num_requests = 0;   /* number of pending requests, initially none */

int main(int argc, char ** argv){
    
    /* Get port number for server to listen on */
    //if (argc != 2) {
    //    printf("usage: game_server port_number\n");
    //    exit(1);
    //}

    /* assign handler for Crtl-C for the parent thread */
    struct sigaction act;
    sigset_t mask, orig_mask; // used for pselect()

    memset(&act, 0, sizeof(act));
    act.sa_handler = handleSIGINT;
    if(sigaction(SIGINT, &act, NULL)){
        perror("sigaction");
        return EXIT_FAILURE;
    }

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);

    /*
     * Allocate and init memory for the players game data
     * */
    client_info * players;
    if((players = malloc(MAX_PLAYERS * sizeof(*players))) == NULL)
        perror("failed to allocate player structs");
    else{
        initPlayers(players);       
        printf("players initd\n");
    }

    if((prev_pckts = malloc(MAX_PLAYERS * sizeof(*prev_pckts))) == NULL)
        perror("failed to allocate prev_pckts");
    else{
        for(uint8_t n = 0; n < MAX_PLAYERS; n++)
            initPrevPckt(&prev_pckts[n]);
    }

    /* Setup thread pool */                                  
    pthread_t p_threads[NUM_THREADS];
    int tid[3] = {0,1,2};   

    /* create handling threads */
	for (uint8_t i = 0; i < NUM_THREADS; i++) {
    	if(pthread_create(&p_threads[i], NULL, (void*)(*handleReqsLoop),(void*)&tid[i])){
            fprintf(stderr, "p_thread[%d] create failed\n", i);
            return EXIT_FAILURE;    
        }
    }    

    if(pthread_mutex_init(&leaderbrd_mutex, &lmut_attr)){
        perror("leader board mutex init failed");
        return EXIT_FAILURE;
    }
    if(pthread_mutexattr_setrobust_np(&lmut_attr, PTHREAD_MUTEX_ROBUST_NP)){
        perror("setting leader board mutex to robust failed");
        return EXIT_FAILURE;
    }
    if(pthread_mutexattr_settype(&lmut_attr, PTHREAD_MUTEX_ERRORCHECK_NP)){
        perror("setting leader board mutex to error-checking failed");
        return EXIT_FAILURE;
    }
    // EOWNERDEAD

    /* listen on a port for client requests */

    int sockfd;  /* listen on sock_fd, new connection on new_fd */

    if (listenOnSocket(&sockfd) != 0)
        exit(EXIT_FAILURE);

    fd_set read_fds;  
    int maxsock = sockfd;
	ssize_t bytes_avail;
    for(uint8_t n = 0; n < MAX_PLAYERS; n++) active_request[n] = false;
    // Authenticate new connections, tell new clients their player_num
    // add requests when packets come in
    while(sigint_recv == 0){ // loop until SIGINT 
        
        maxsock = refreshFdSets(&read_fds, sockfd, players);

        /* ** <prevent SIGINT race condition when blocking on pselect> ** */
        if (pthread_sigmask(SIG_BLOCK, &mask, &orig_mask) < 0){
            perror ("pthread_sigmask");
            return EXIT_FAILURE;
        }

        int status = pselect(maxsock + 1, &read_fds, NULL, NULL, NULL, &orig_mask); // timeval = NULL => blocking

        printf("status %d\n", status);
        if (pthread_sigmask(SIG_UNBLOCK, &mask, &orig_mask) < 0){
            perror ("pthread_sigmask");
            return EXIT_FAILURE;
        }
        /* ** </prevent SIGINT race condition when blocking on pselect> ** */

        if(sigint_recv) break; // if SIGINT happened whilst blocking on pselect(), then get out immediately
        if(status < 0) perror("pselect()"); //TODO: fprintf TIMESTAMP here
        else if(status > 0){
            if(FD_ISSET(sockfd, &read_fds)) 
                // do not use worker threads to accept new connections since there is only one sockfd 
                acceptNewConnection(players, sockfd);
            else{
                for(uint8_t n = 0; n < MAX_PLAYERS; n++){
                    if(!active_request[n])
                        if(players[n].fd != NO_SOCKET  && FD_ISSET(players[n].fd, &read_fds)){
                            ioctl(players[n].fd, FIONREAD, &bytes_avail);
                            if(bytes_avail > 0){
                                active_request[n] = true;
                                addReq(&players[n]);
                            }
                    }
                }
            }
        }
	}

    sigint_thrds = true;
    printf("\nSIGINT triggered!\n"); fflush(stdout);
    /* CLEAN-UP */  

    // rejoin the threads - threads see sigint_recv as well
    for(uint8_t i = 0; i < NUM_THREADS; i++){
        if(pthread_join(p_threads[i], NULL)) 
            perror("pthread_join");
        printf("thread num joined: %d\n", i); fflush(stdout);
    }
    printf("threads all joined\n"); fflush(stdout);
    // normal warnings against destroying locked mutexes
    // should not apply here as the whole server is 
    // shutting down
    pthread_mutex_destroy(&leaderbrd_mutex);        
    pthread_mutexattr_destroy(&lmut_attr);    
    pthread_mutex_destroy(&request_mutex);

    destroyPlayers(players);

    for(uint8_t n = 0; n < MAX_PLAYERS; n++)
            free(prev_pckts[n].data);

    for(uint8_t n = 0; n < MAX_PLAYERS; n++) 
        if(players[n].fd != NO_SOCKET) 
            close(players[n].fd); // SHUTDOWN was called earlier

    free(players);
    free((void*)prev_pckts);

    close(sockfd);

    return EXIT_SUCCESS;
}

static void handleSIGINT(int sig){
    sigint_recv = 1;
}

/* ======================================================= */
/* ============ <from inside a Worker Thread> ============ */

uint8_t acquireLeaderMutex(void){
    int mac = 0; // mutex availability code
    int con = 0; // consistent
    int rc = 0; // return code
    // NB: this function requires that the leaderbrd_mutex
    // be ERROR_CHECKING and ROBUST
    int iter = 0;
    do{
        printf("attempt to acquire leader mutex, iter: %d\n", iter++);
        fflush(stdout);
        
        if((mac = pthread_mutex_lock(&leaderbrd_mutex)) \
            == ENOTRECOVERABLE){
            // ENOTRECOVERABLE will happen if a previous thread
            // encountering EOWNERDEAD failed to restore the 
            // correct state that the mutex was guarding
            // and instead called pthread_mutex_unlock before
            // calling pthread_mutex_consistent
            perror("leader mutex unrecoverable");
            // try to shutdown gracefully
            if(kill(getpid(), SIGINT)) exit(EXIT_FAILURE); 
            return LMUT_BREAK_CLEAN;
        }
    }while(!sigint_recv && mac != EOWNERDEAD && mac == EDEADLK); 
                
    if(mac == EOWNERDEAD){
        printf("leader mutex former owner died.\n");
        fflush(stdout);
        if(con = pthread_mutex_consistent(&leaderbrd_mutex))
            perror("pthread_mutex_consistent");
        else if(con = pthread_mutex_lock(&leaderbrd_mutex)) 
            perror("pthread_mutex_lock");

        if(con != 0) return LMUT_BREAK_AGAIN; 
        else if(!sigint_recv){
        // on the off chance a rare race condition with the signal has occured
            if(rc = pthread_mutex_unlock(&leaderbrd_mutex)){
                if(rc != EDEADLK){
                    // returns EDEADLK if not owner thread, this is not considered an error
                    perror("pthread_mutex_unlock failed but not from EDEADLK");    
                    return LMUT_BREAK_CLEAN;
                }
            } 
            printf("returning LMUT_BREAK_AGAIN.\n");
            fflush(stdout);
            return LMUT_BREAK_AGAIN;
        }
        else return LMUT_BREAK_CLEAN; 
        // better to break than attempt to access a single
        // shared memory space in the midst of a server shutdown
    }

    return LMUT_OK;
}

/*
 * function handleReq(): handle a single given request
 * input:     request pointer.
 * output:    none.
 */
void handleReq(request_t * a_request){
    // this is the server side game and menu controller
    unsigned char packet[MAX_PACKET_LEN];
    int curr_fd = a_request->player->fd;
    uint8_t curr_id = a_request->player->id;
    printf("handling request. getting packet from: %d, on socket: %d\n",curr_id,curr_fd);
    fflush(stdout);
    int8_t pc = getPacket(false, ISSERVER, curr_fd, packet);

    if(pc == CONN_OFF){
        fprintf(stderr, "player %d connection on socket %d shutdown normally\n", curr_id, curr_fd);
        fflush(stdout);
        close(curr_fd);
        a_request->player->fd = NO_SOCKET;
    }
    else if(pc == CONN_ERR){
        fprintf(stderr, "player %d connection on socket %d lost with error\n", curr_id, curr_fd);
        fflush(stdout);
        close(curr_fd);
        a_request->player->fd = NO_SOCKET;
    }
    else if(pc == CHIRP && packet[CHIRP_NAME] == '+'){
        unsigned char verb = prev_pckts[curr_id].verb;
        unsigned char * data = prev_pckts[curr_id].data;
        uint16_t data_len = prev_pckts[curr_id].data_len;
        printf("resending packet due to recv chirp +\n");
        fflush(stdout);
        sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], verb, data, data_len);
    }
    else if(pc == INVLD_PCKT){
        printf("asking for resend, chirp +\n");
        fflush(stdout);
        sendChirp(curr_fd, '+', 0);
    }
    else if(pc == STD_PCKT){
        uint8_t rc, x, y;
        unsigned char coords[2];
        int8_t update_code;
        uint8_t * data;
        printf("STD_PCKT recv, entering FSM.\n");
        fflush(stdout);
        switch(packet[PCKT_VERB]){ // VERB

            // START NEW GAME
            case 'S': 
                printf("S\n"); fflush(stdout);
                sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'S', 0, 0);
                break;

            // VIEW LEADER-BOARD
            case 'L':
                printf("L\n"); fflush(stdout);
                if((rc = acquireLeaderMutex()) == LMUT_BREAK_CLEAN)   
                    break; // problem wasn't resolved this cycle
                else if(rc == LMUT_BREAK_AGAIN){
                    // prompt client to resend last packet
                    printf("asking for resend, chirp +, LMUT_BREAK_AGAIN\n");
                    fflush(stdout);
                    sendChirp(curr_fd, '+', 0);
                    break;
                }
                printf("sending L\n"); fflush(stdout);
                sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'L', 0, 0);

                rc = pthread_mutex_unlock(&leaderbrd_mutex);
                printf("leader mutex released\n"); fflush(stdout);
                break;

            // QUIT WHOLE GAME
            case 'Q': 
                printf("Q\n"); fflush(stdout);
                shutdown(curr_fd, SHUT_RDWR);
                close(a_request->player->fd);
                a_request->player->fd = NO_SOCKET;
                break;

            // QUIT CURRENT MINEFIELD
            case 'q': 
                printf("q\n"); fflush(stdout);
                sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'q', 0, 0);
                break;

            // PLACE FLAG, must have data "x,y"
            case 'P': 
                printf("P\n"); fflush(stdout);
                // get "x,y"
                x = 0; y = 0;
                coords[0] = x; coords[1] = y;
                sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'X', coords, 2);
                //sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'W', coords, 2);
                //sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'Y', coords, 2);
                break;

            // REVEAL TILE, must have data "x,y"
            case 'R': 
                printf("R\n"); fflush(stdout);
                // get "x,y"
                x = 0; y = 0;    
                data = malloc(3 * sizeof(*data));
                data[0] = x; data[1] = y; data[2] = 0; 
                sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'C', data, 3);    
                //data = malloc(2 * sizeof(*data));
                //data[0] = x; data[1] = y; 
                //sendPacket(curr_fd, curr_id, &prev_pckts[curr_id], 'D', data, 2);
                free(data);                    
                break;

            default:
                perror("validPacket() must not have worked correctly");
                break;
        }
    }
    // reset active request status
    active_request[curr_id] = false;
}

/*
 * function getReq(): gets the first pending request from the requests list
 *                         removing it from the list.
 * algorithm: creates a request structure, adds to the list, and
 *            increases number of pending requests by one.
 * input:     request number, linked list mutex.
 * output:    pointer to the removed request, or NULL if none.
 * memory:    the returned request need to be freed by the caller.
 */
request_t * getReq(pthread_mutex_t * request_mutex){

    request_t * a_request;      /* DO I NEED TO MALLOC HERE? */

    /* lock the mutex, to assure exclusive access to the list */
    if(pthread_mutex_lock(request_mutex)) perror("pthread_mutex_lock");

    printf("request mutex acquired\n");
    if (num_requests > 0) {
        a_request = requests;
        requests = a_request->next;
        if (requests == NULL) { /* this was the last request on the list */
            last_request = NULL;
        }
        /* decrease the total number of pending requests */
        num_requests--;
        printf("request taken. Num left: %d\n", num_requests);
    }
    else { /* requests list is empty */
        a_request = NULL;
        printf("no requests available at the moment\n");
    }

    /* unlock mutex */
    if(!pthread_mutex_unlock(request_mutex)) perror("pthread_mutex_unlock");

    printf("request mutex released\n");
    fflush(stdout);
    /* return the request to the caller. */
    return a_request;
}

/*
 * function handleReqsLoop(): infinite loop of requests handling
 * algorithm: forever, if there are requests to handle, take the first
 *            and handle it. Then wait on the given condition variable,
 *            and when it is signaled, re-do the loop.
 *            increases number of pending requests by one.
 * input:     id of thread, for the request handler.
 * output:    none.
 */
void * handleReqsLoop(void * data){
    
    sigset_t mask; // used for pselect()
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) < 0){
        perror ("pthread_sigmask");
        exit(EXIT_FAILURE);
    }
    else printf("blocking\n");
    fflush(stdout);

    request_t * a_request = NULL;      /* pointer to a request.*/
    
    /* do until interrupted */
    while (!sigint_thrds){
        if (num_requests > 0){ /* a request is pending */
            a_request = getReq(&request_mutex);
            if (a_request){ /* got a request - handle it and free it */
                printf("request gotten. player: %d, on sock: %d\n",a_request->player->id,a_request->player->fd);
                
                handleReq(a_request);
                printf("request handled\n"); fflush(stdout);
                if(!sigint_thrds){
                     free(a_request);
                     a_request = NULL;
                }
            }
        }
    }
    
    if(a_request != NULL){
        sendChirp(a_request->player->fd, '*', 0);
        printf("send chirp *\n"); fflush(stdout);
        // if request had been nominally fulfilled, then this packet will be ignored
        // if request couldn't be satisfied and thread gets here, the listening client is
        // informed of the shut-down, and (most importantly) able to return promptly from recv()
        free(a_request);
    }
}

/* ============ </from inside a Worker Thread> ============ */
/* ======================================================== */



/* ========================================================== */
/* ============ <from inside the Primary Thread> ============ */

/*
 * function addReq(): add a request to the requests list
 * algorithm: creates a request structure, adds to the list, and
 *            increases number of pending requests by one.
 * input:     request struct fields, linked list mutex.
 * output:    none.
 * precondition: ONLY THREAD TO CALL IS PARENT THREAD (BOSS)
 */
void addReq(client_info * player){
    
    request_t * a_request;      /* pointer to newly added request.     */

    /* create structure with new request */
    if(!(a_request = malloc(sizeof(*a_request)))){ /* malloc failed?? */
        perror("add_request: out of memory");
        exit(1);
    }

    a_request->player = player;
    a_request->next = NULL;
    
    /* add new request to the end of the list, updating list */
    /* pointers as required */
    if (num_requests == 0) { /* special case - list is empty */
        requests = a_request;
        last_request = a_request;
        printf("request queue was empty\n"); fflush(stdout);
    }
    else {
        last_request->next = a_request;
        last_request = a_request;
    }

    /* increase total number of pending requests by one. */
    num_requests++;
    printf("request queue grew to %d\n", num_requests); fflush(stdout);
}

void initPlayers(client_info * players){
    
    /*
     * Allocate memory for the full map of data needed for up to MAX_PLAYERS
     * 10*9*9 Bytes
     * */
    int8_t *** map; 
    if(map = malloc(MAX_PLAYERS * sizeof(*map))){
        for(uint8_t n = 0; n < MAX_PLAYERS; n++){
            if(map[n] = malloc(NUM_TILES_Y * sizeof(**map))){
                for(uint8_t j = 0; j < NUM_TILES_Y; j++){ 
                    if((map[n][j] = malloc(NUM_TILES_X * sizeof(***map))) == NULL)
                        fprintf(stderr, "\nfailed to allocate map[%d][%d]\n", n, j); 
                }
            }
            else fprintf(stderr, "\nfailed to allocate map[%d]\n", n);
        }
    } 
    else perror("\nfailed to allocate map\n");
    
    /*
     * Allocate memory for partial map visible to client
     * 10*9*9 Bytes
     * */
    unsigned char *** part_map;
    if(part_map = malloc(MAX_PLAYERS * sizeof(*part_map))){
        for(uint8_t n = 0; n < MAX_PLAYERS; n++){
            if(part_map[n] = malloc(NUM_TILES_Y * sizeof(**part_map))){
                for(uint8_t j = 0; j < NUM_TILES_Y; j++){ 
                    if((part_map[n][j] = malloc(NUM_TILES_X * sizeof(***part_map))) == NULL)
                        fprintf(stderr, "\nfailed to allocate part_map[%d][%d]\n", n, j);
                    else for(uint8_t i = 0; i < NUM_TILES_X; i++) part_map[n][j][i] = '_'; 
                }
            }
            else fprintf(stderr, "\nfailed to allocate part_map[%d]\n", n); 
        }
    }else perror("\nfailed to allocate part_map\n");

    unsigned char ** names;
    if(names = malloc(MAX_PLAYERS * sizeof(*names))){
        for(uint8_t n = 0; n < MAX_PLAYERS; n++){
            if((names[n] = malloc(MAX_AUTH_FIELD_LEN * sizeof(**names))) == NULL)
                fprintf(stderr, "\nfailed to allocate names[%d]\n", n);
        }
    }
    else perror("\nfailed to allocate names\n");

    unsigned char ** pwords;
    if(pwords = malloc(MAX_PLAYERS * sizeof(*pwords))){
        for(uint8_t n = 0; n < MAX_PLAYERS; n++){
            if((pwords[n] = malloc(MAX_AUTH_FIELD_LEN * sizeof(**pwords))) == NULL)
                fprintf(stderr, "\nfailed to allocate pwords[%d]\n", n);
        }
    }
    else perror("\nfailed to allocate pwords\n");

    pullAuthFields(names, pwords);

    // initialize players structs with pointers to map and part_map, etc
    for(uint8_t n = 0; n < MAX_PLAYERS; n++){
        players[n].id = n;
        players[n].fd = NO_SOCKET;
        players[n].name = names[n];
        players[n].pword = pwords[n];
        players[n].part_map = part_map[n];
        players[n].map = map[n];
        players[n].mines_rem = NUM_MINES;
        players[n].start_time = 0;
        srand48_r(RAND_NUM_SEED, &players[n].randBuffer);
    }
}

int8_t authConnection(client_info * players, int new_fd){
    int8_t player_num = AUTH_FAIL; // client is not yet authorised
    unsigned char packet[MAX_PACKET_LEN];
    sendChirp(new_fd, '?', 0); // WHO ARE YOU?
    printf("@ %d, chirp: ?\n", new_fd);

    int8_t pc = getPacket(true, ISSERVER, new_fd, packet);
    int i = PCKT_DATA;
    do{
        printf("%2x ", packet[i++]);     
    }while(packet[i] != ETX);
    printf("\n");

    if(pc == CHIRP){
        if(packet[CHIRP_NAME] == '+'){
            sendChirp(new_fd, '?', 0); // WHO ARE YOU? 
            printf("@ %d, chirp: ?, after recv +\n", new_fd);   
        } 
        else perror("client replied with invalid chirp name");
    }
    else if(pc == INVLD_PCKT){
        uint8_t cnt = 0;
        do{
            sendChirp(new_fd, '+', 0); // SEND AGAIN
            printf("@ %d, chirp: +\n", new_fd);
            pc = getPacket(true, ISSERVER, new_fd, packet);    
        }while(++cnt < RESEND_LIM && pc != STD_PCKT);
    }
     // TODO: deal with situation where client sends INVALID auth_fields
    if(pc == STD_PCKT){
        bool next = false; uint8_t idx = PCKT_DATA;
        uint8_t name_len = 0;
        unsigned char name[MAX_AUTH_FIELD_LEN]; unsigned char pword[MAX_AUTH_FIELD_LEN];
        while(!next && (packet[idx] != ETX) && (idx < (MAX_AUTH_FIELD_LEN + PCKT_DATA))){ // getting name
            
            if(packet[idx] == SEP){
                next = true;
                printf("!");    
            }
            else name[name_len++] = packet[idx]; 
            printf("%c",name[name_len-1]);
            idx++;
        }
        printf("\n");
        for(int ii = 0; ii < name_len; ii++) printf("%c",name[ii]);
        printf("\n");

        uint8_t pword_len = 0;
        while(next && (packet[idx] != ETX)){
            pword[pword_len++] = packet[idx];
            printf("%2x ", pword[pword_len-1]);
            idx++; // getting password    
        } 
        printf("\n");
        for(int ii = 0; ii < pword_len; ii++) printf("%c",pword[ii]);
        printf("\n");
        if(next){ // authenticate
            // HERE!
            // check name is in auth table
            for(uint8_t n = 0; n < MAX_PLAYERS; n++){
                player_num = n;
                for(uint8_t p = 0; p < name_len; p++){
                    // ? name matches on all humanReadable chars                    
                    if(name[p] != players[n].name[p]){
                        printf("%c\n", players[n].name[p]);
                        printf("%c\n", name[p]);
                        printf("\n");
                        player_num = AUTH_FAIL;
                        break;
                    } 
                }
                if(player_num == n){
                    // ? pword matches on all humanReadable chars
                    for(uint8_t p = 0; p < pword_len; p++){
                        if(pword[p] != players[n].pword[p]){
                            player_num = AUTH_FAIL;
                            break;
                        }
                    }
                    if(player_num == AUTH_FAIL){
                        sendChirp(new_fd, 'D', 0);
                        printf("@ %d, chirp: D\n", new_fd);                        
                        break; // leaving outer loop from failed pword    
                    } 
                    
                }
                else continue;
                break; // leaving outer loop from matched pword
            }
        }
    } 
    
    if(player_num != AUTH_FAIL && players[player_num].fd != NO_SOCKET){
        player_num = AUTH_FAIL;
        sendChirp(new_fd, '|', 0); // advise there is a client already logged on at this socket
        printf("@ %d, chirp: |\n", new_fd);
    }

    fflush(stdout);
    return player_num;
}

/* accepting the new connection */
void acceptNewConnection(client_info * players, int sockfd){

    //struct sockaddr_in client_addr;
    //memset(&client_addr, 0, sizeof(client_addr));
    //socklen_t client_len = sizeof(client_addr);
    //int new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    int new_fd = accept(sockfd, NULL, NULL);
    if (new_fd < 0){
        perror("accept()");
        exit(EXIT_FAILURE);
    }
    printf("connection from client accepted on %d: \n", new_fd);
    //char client_ipv4_str[INET_ADDRSTRLEN];
    //inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4_str, INET_ADDRSTRLEN);
  
    //printf("Incoming connection from %s:%d.\n", client_ipv4_str, client_addr.sin_port);
  
    /* Authenticate the new client */
    int8_t player_num = authConnection(players, new_fd);
    
    if(player_num == AUTH_FAIL){
        perror("client login attempt failed");
        close(new_fd);
        return;
    }
    players[player_num].fd = new_fd;

    sendChirp(new_fd, 'A', player_num);

    printf("connection from client (player %d) authorized on %d: \n", player_num, players[player_num].fd);
    fflush(stdout);
}

void destroyPlayers(client_info * players){

    // free allocated memory 
    for(uint8_t n = 0; n < MAX_PLAYERS; n++){
        if(players[n].fd != NO_SOCKET) 
            shutdown(players[n].fd, SHUT_RD); // send FIN packets
        for(uint8_t j = 0; j < NUM_TILES_Y; j++){
            free(players[n].map[j]);
            free(players[n].part_map[j]);
        }
        free(players[n].map);
        free(players[n].part_map);
        free(players[n].name);
        free(players[n].pword); 
    }
    // rather than SO_LINGER, if remaining exit seq starts before shutdown FIN...too bad
}

int refreshFdSets(fd_set * read_fds, int sockfd, client_info * players){
    printf("refreshing FdSets\n");
    FD_ZERO(read_fds);
    FD_SET(sockfd, read_fds);
    for (uint8_t n = 0; n < MAX_PLAYERS; n++)
        if (players[n].fd != NO_SOCKET){
             FD_SET(players[n].fd, read_fds);    
             printf("setting read_fs on player: %d\n", n);
        }

    int maxsock = sockfd;
        for(uint8_t n = 0; n < MAX_PLAYERS; n++) 
            if(players[n].fd > maxsock){
                maxsock = players[n].fd;    
                printf("new maxsock: %d, from player: %d\n", maxsock, n);
            } 
    
    fflush(stdout);
    return maxsock;
}

/* create socket, setsockopt, bind, listen */
int8_t listenOnSocket(int * sockfd){
    // get file descriptor for port to listen on
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        perror("socket()");
        return NO_SOCKET;
    }
    printf("listening on socket: %d\n", *sockfd);
   
    int reuse = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        perror("setsockopt()");
        return RET_ERR;
    }
    printf("setting options to reuse old sockets. \n");
    
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr(SERVER_IPV4_ADDR);
    my_addr.sin_port = htons(SERVER_LISTEN_PORT);
    
    if (bind(*sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) != 0) {
        perror("bind()");
        return RET_ERR;
    }
    printf("bound on socket: %d\n", *sockfd);

    // start listening
    if (listen(*sockfd, BACKLOG) != 0) {
        perror("listen()");
        return RET_ERR;
    }
    printf("Listening on port %d.\n", (int)SERVER_LISTEN_PORT);
    fflush(stdout);
    return 0;
}

/* ============ </from inside the Primary Thread> ============ */
/* =========================================================== */