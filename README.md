### Description

Client-Server style CLI Minesweeper Game written in C for Linux. Featuring:

1. Unix Sockets
2. Signal handling
3. Pthreads in a thread pool configuration and protected critical sections
4. Highly optimized minefield using connected-component-labelling (blob detection); 1 byte per tile
5. Client authentication (of the pointless and weak variety) and a leader-board

### Getting Started

`$ cd src`

`$ cp ../Makefile .`

`$ make`

`$ ./server`

Open a new terminal, navigate to the same folder

`$ ./client 127.0.0.1 33333`

Pick a name and password from the `Authentication.txt` file for login credentials.

To run the server-client connection across different machines, use the correct IP address instead of `127.0.0.1` and make sure port forwarding is setup correctly on `33333`

### Data-structure that Represents the Playing Field

An 81 byte array. Tiles with numbers 1-8 under them, are just valued 1-8. Mines are indicated using INT8 MIN (-128). Tiles with a zero directly underneath are represented with a number 9 through INT8 MAX, and all contiguous zero tiles share the same number. Tiles adjacent to a contiguous grouping of zero tiles with some number 1-8 under them are represented using the numbers -9 through (INT8 MIN + 1), where the number in the tens position indicates which contiguous blob of zeros the tile is adjacent to, and the value in the ones position holds the value of the number under the tile that must be revealed.

This is **connected-component labelling**, sometimes known as blob detection. It does involve a more computationally heavy setup procedure, however, once the field is setup per game turn computation is reduced and the data becomes a trivial speck in a cache line. **Stay on the Cache!**

Whilst this is an ideal solution for a 9x9 field, one would be advised to switch to using int16 t values in the array for anything larger than 9x9, as more than 12 contiguous regions of zero may start to become possible.

### Data-structure that Represents the Leaderboard

```
typedef struct {
  uint8_t num_entries; //MAX = 21 = MAXLRDBRDENTRIES
  uint8_t ∗ best_ids ;
  time_t ∗ best_times; // IT WAS THE BEST OF TIMES, IT WAS THE BLURST OF TIMES! stupid monkey
  uint16_t ∗ player_stats; // {num_won, num_played, ...} @ idx1 = 2∗id, idx2 = 2∗id+1
  unsigned char ∗∗ playernames; // len = MAXPLAYERS ∗ MAXAUTHFIELDLEN, idx = id
  bool vld; // for error checking and backups
} leaderbrd ;
```
So you see now, there’s this struct thing, and people who don’t prefer to write in assembly seem to really like them. I’ve no idea why. C is weakly typed anyway. You’ve got bit-wise operators right? Use them.

### Unit tests

#### Minefield

Encode all the necessary data in as few bits as you can, offload as much structural information to some simple arithmetic patterns, and pack it into a contiguous array.

Rather than chasing pointers all over the heap like some sort of linked-list loving, cycle-wasting programmer who makes things easy on themselves.

Granted, setting up the original mine-field is computationally heavier than building a linked-list. However, once built, the encoding follows a simple to decode pattern.

1. Mines are the lowest available signed value (for 8-bits = -128)
2. Non mine tiles have values 0 - 8 as per usual game rules
3. 0 tiles are assigned a number 9 - MAX based on which contiguous blob of 0s they belong to
4. Non-zero tiles adjacent to a blob of zeros go into -ve values
5. The ones position of -ve valued non-zero (non-mine) tiles retain the tiles original value
6. The tens position is based on which blob of zeros a -ve value is adjacent to

![](https://i.imgur.com/i4wDj93.png)

The final process of evaluated moves and revealing blobs on the minefield is simple and avoids visiting the heap.

Expanding zero-blobs is done by:

```
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
```

And other checks for hitting a mine or not are trivial.
