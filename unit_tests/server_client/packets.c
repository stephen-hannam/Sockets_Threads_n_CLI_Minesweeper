#include "packets.h"

void sendChirp(int fd, unsigned char name, uint8_t player_num){
    unsigned char chirp[CHIRP_LEN];
    chirp[CHIRP_LEN0] = 0x00; chirp[CHIRP_LEN1] = 0x00;
    chirp[CHIRP_NAME] = name; chirp[CHIRP_DATA] = 0xFF;
    if(name == 'A') chirp[CHIRP_DATA] = player_num;
    send(fd, chirp, CHIRP_LEN, 0);
}

bool validChirp(unsigned char C){
    switch(C){
        case 'A':
        case '?':
        case 'D':
        case '*':
        case '|':
        case '+':
            return true;
        default:
            return false;
    }
}

void sendPacket(int fd, uint8_t player_num, volatile prev_packet * prev_pckt, \
                const unsigned char verb, unsigned char * data, uint16_t data_len){
    
    unsigned char packet[MAX_PACKET_LEN];
    ssize_t num_sent = 0;
    ssize_t total_sent = 0;
    uint16_t total_len = data_len + NUM_FRAME_BYTES;

    packet[PCKT_LEN0] = (data_len + NUM_FRAME_BYTES) & 0x00FF;
    packet[PCKT_LEN1] = ((data_len + NUM_FRAME_BYTES) & 0xFF00) >> 8;
    packet[PCKT_ID] = player_num;
    packet[PCKT_VERB] = verb;
    prev_pckt->verb = verb;
    prev_pckt->data_len = data_len;

    for(uint16_t i = 0; i < data_len; i++){
        packet[PCKT_DATA + i] = data[i];
        prev_pckt->data[i] = data[i];
    }
    
    packet[PCKT_DATA + data_len] = ETX;
    do{
        num_sent = send(fd, packet, total_len, 0);
        if(num_sent < 0){
            int errsv = errno;
            fprintf(stderr, "err: %d\n", errsv);
            break;    
        }
    }while((total_sent += num_sent) < total_len);
}

int8_t getPacket(bool new_conn, bool isserver, int fd, unsigned char packet[MAX_PACKET_LEN]){
    bool replied; 
    uint16_t exp_len = 0; // expected packet length based on packet header
    ssize_t num_recv;
    ssize_t total_recv = 0;

    do{
        replied = true;
        num_recv = recv(fd, (unsigned char*)&packet[total_recv], MAX_PACKET_LEN, 0);
        if(num_recv < 0){
            int errsv = errno;
            fprintf(stderr, "err: %d\n", errsv);
            return CONN_ERR;    
        }
        else if(num_recv == 0) return CONN_OFF;        
        else{
            // packets begin with a packet length header in 2 bytes
            if((total_recv += num_recv) > 1 && (exp_len == 0))
                if((exp_len = packet[PCKT_LEN0] + (packet[PCKT_LEN1] << 8)) == 0)
                    if(validChirp(packet[CHIRP_NAME])) return CHIRP; 
            if(total_recv < exp_len) replied = false; // more should be expected
        }
    }while(!replied);

    if(total_recv > 0) 
        if(validPacket(new_conn, isserver, packet, exp_len)) 
            return STD_PCKT;
    return INVLD_PCKT;
}

bool validPacket(bool new_conn, bool isserver, unsigned char * packet, uint16_t exp_len){
    // server and client packets follow some different rules
    bool valid = true; // these are hard/fast tests of form, not content
    int8_t pnum = AUTH_FAIL;
    
    //for(uint8_t i = 0; i < exp_len; i++) printf("%x ", packet[i]);
    //printf("\n");

    if(exp_len > MAX_PACKET_LEN) valid = false;
    else if(packet[exp_len - 1] != ETX) valid = false;

    pnum = packet[PCKT_ID];
    if(valid){
        if(new_conn && isserver){
            // two auth fields + SEP + frame bytes
            if(exp_len > 2*MAX_AUTH_FIELD_LEN + 1 + NUM_FRAME_BYTES) valid = false; 
        }
        else if(isserver){ // Server checks if packet from Client is valid
            if(pnum < 0 || pnum > MAX_PLAYERS - 1) valid = false; //player_num
            else{
                switch(packet[PCKT_VERB]){ // verb
                    case 'S': // Start new game
                    case 'L': // request leaderboard
                    case 'Q': // Quit entire game, close connection
                    case 'q': // quit current minefield
                        if(exp_len != NUM_FRAME_BYTES) valid = false; // strictly no data
                        break;
                    case 'P': // place flag
                    case 'R': // reveal tile
                        if(exp_len != NUM_FRAME_BYTES + 2) valid = false; // strictly 2 fields of data
                        break;
                    default:
                        valid = false;
                }
            }
        }
        else{ // Client checks if packet from Server is valid
            if(pnum < 0 || pnum > MAX_PLAYERS - 1) valid = false; //player_num
            else{
                switch(packet[PCKT_VERB]){ // verb
                    case 'S': // confirm server has init'd a new game                    
                    case 'q': // server confirms minefield has been deleted after client quits
                        if(exp_len != NUM_FRAME_BYTES) valid = false; // strictly no data
                        break;
                    case 'X': // flag placement invalid, no mine at x,y
                    case 'Y': // flag placed at x,y
                        if(exp_len != NUM_FRAME_BYTES + 2) valid = false; // strictly 2 fields of data
                        break;
                    case 'C': // char value of tile @ x,y
                        if(exp_len != NUM_FRAME_BYTES + 3) valid = false; // strictly 3 fields of data
                        break;
                    case 'M': // blob reveal, data to follow is new part_map
                    case 'D': // dead, new part_map with all mines shown
                        if(exp_len < NUM_FRAME_BYTES + NUM_TILES) valid = false; // min 9x9 fields of data
                        break;
                    case 'L': // variable length of data to follow
                    case 'W': // player wins, variable data length = time in seconds as char array
                        break;
                    default:
                        valid = false;
                }
            }
        }
    }
    if(!valid) fprintf(stderr, "player %d sent an invalid packet", pnum);
    return valid;
}

void initPrevPckt(volatile prev_packet * prev_pckt){
    
    unsigned char * data;
        
    if((data = malloc((MAX_PACKET_LEN - NUM_FRAME_BYTES + 1)*sizeof(*data))) == NULL)
        perror("failed to allocate prev_pckt data");
    prev_pckt->verb = 0;
    prev_pckt->data = data;
    prev_pckt->data_len = 0;
}