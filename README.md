### Description

Client-Server style CLI Minesweeper Game written in C for Linux. Featuring:

1. Unix Sockets
2. Signal handling
3. Pthreads in a thread pool configuration and protected critical sections
4. Highly optimized minefield using connected-component-labelling (blob detection); 1 byte per tile

	-- rather than chasing pointers all over the heap like some sort of link-list loving, cycle-wasting programmer who makes things easy on themselves.

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

![](https://i.imgur.com/i4wDj93.png)
