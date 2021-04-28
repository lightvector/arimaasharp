
#include "../core/global.h"
#include "../core/hash.h"
#include "../core/rand.h"
#include "../board/board.h"

//STATIC DATA MEMBERS-----------------------------------------------------------------------

hash_t Board::HASHPIECE[2][NUMTYPES][BSIZE];
hash_t Board::HASHPLA[2];
hash_t Board::HASHSTEP[4];

Bitmap Board::GOALMASKS[2];
bool Board::ISADJACENT[DIFFIDX_SIZE];
int Board::MANHATTANDIST[DIFFIDX_SIZE];
int Board::DIRTORFAR[DIFFIDX_SIZE];
int Board::DIRTORNEAR[DIFFIDX_SIZE];

int8_t Board::SPIRAL[BSIZE][64];
Bitmap Board::RADIUS[16][BSIZE];
Bitmap Board::DISK[16][BSIZE];
Bitmap Board::GPRUNEMASKS[2][BSIZE][5];

Bitmap Board::XINTERVAL[8][8];
Bitmap Board::YINTERVAL[8][8];


//INLINE TABLES----------------------------------------------------
static const int8_t ESQ = ERRLOC;

const int8_t Board::TRAPLOCS[4] = {TRAP0,TRAP1,TRAP2,TRAP3};

const int8_t Board::TRAPDEFLOCS[4][4] = {
{TRAP0 S,TRAP0 W,TRAP0 E,TRAP0 N},
{TRAP1 S,TRAP1 W,TRAP1 E,TRAP1 N},
{TRAP2 S,TRAP2 W,TRAP2 E,TRAP2 N},
{TRAP3 S,TRAP3 W,TRAP3 E,TRAP3 N},
};

const int8_t Board::TRAPBACKLOCS[4] = {TRAP0 S, TRAP1 S, TRAP2 N, TRAP3 N};
const int8_t Board::TRAPOUTSIDELOCS[4] = {TRAP0 W, TRAP1 E, TRAP2 W, TRAP3 E};
const int8_t Board::TRAPINSIDELOCS[4] = {TRAP0 E, TRAP1 W, TRAP2 E, TRAP3 W};
const int8_t Board::TRAPTOPLOCS[4] = {TRAP0 N, TRAP1 N, TRAP2 S, TRAP3 S};

const int Board::PLATRAPINDICES[2][2] = {{2,3},{0,1}};
const int8_t Board::PLATRAPLOCS[2][2] = {{TRAP2,TRAP3},{TRAP0,TRAP1}};

const bool Board::ISPLATRAP[4][2] =
{{false,true},{false,true},{true,false},{true,false}};

const int Board::TRAPINDEX[BSIZE] =
{
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ, 0 ,ESQ,ESQ, 1 ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ, 2 ,ESQ,ESQ, 3 ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
};

const bool Board::ISTRAP[BSIZE] =
{
false,false,false,false,false,false,false,false,  false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,  false,false,false,false,false,false,false,false,
false,false,true ,false,false,true ,false,false,  false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,  false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,  false,false,false,false,false,false,false,false,
false,false,true ,false,false,true ,false,false,  false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,  false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,  false,false,false,false,false,false,false,false,
};

const bool Board::ISEDGE[BSIZE] =
{
true ,true ,true ,true ,true ,true ,true ,true ,  false,false,false,false,false,false,false,false,
true ,false,false,false,false,false,false,true ,  false,false,false,false,false,false,false,false,
true ,false,false,false,false,false,false,true ,  false,false,false,false,false,false,false,false,
true ,false,false,false,false,false,false,true ,  false,false,false,false,false,false,false,false,
true ,false,false,false,false,false,false,true ,  false,false,false,false,false,false,false,false,
true ,false,false,false,false,false,false,true ,  false,false,false,false,false,false,false,false,
true ,false,false,false,false,false,false,true ,  false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,  false,false,false,false,false,false,false,false,
};

const bool Board::ISEDGE2[BSIZE] =
{
true ,true ,true ,true ,true ,true ,true ,true ,  false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,  false,false,false,false,false,false,false,false,
true ,true ,false,false,false,false,true ,true ,  false,false,false,false,false,false,false,false,
true ,true ,false,false,false,false,true ,true ,  false,false,false,false,false,false,false,false,
true ,true ,false,false,false,false,true ,true ,  false,false,false,false,false,false,false,false,
true ,true ,false,false,false,false,true ,true ,  false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,  false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,  false,false,false,false,false,false,false,false,
};

const int Board::EDGEDIST[BSIZE] =
{
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,1,1,1,1,1,1,0,  0,0,0,0,0,0,0,0,
0,1,2,2,2,2,1,0,  0,0,0,0,0,0,0,0,
0,1,2,3,3,2,1,0,  0,0,0,0,0,0,0,0,
0,1,2,3,3,2,1,0,  0,0,0,0,0,0,0,0,
0,1,2,2,2,2,1,0,  0,0,0,0,0,0,0,0,
0,1,1,1,1,1,1,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
};

const int8_t Board::ADJACENTTRAP[BSIZE] =
{
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ, TRAP0,ESQ,ESQ, TRAP1,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ, TRAP0,ESQ, TRAP0, TRAP1,ESQ, TRAP1,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ, TRAP0,ESQ,ESQ, TRAP1,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ, TRAP2,ESQ,ESQ, TRAP3,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ, TRAP2,ESQ, TRAP2, TRAP3,ESQ, TRAP3,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ, TRAP2,ESQ,ESQ, TRAP3,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
};

const int Board::ADJACENTTRAPINDEX[BSIZE] =
{
-1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
-1,-1, 0,-1,-1, 1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
-1, 0,-1, 0, 1,-1, 1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
-1,-1, 0,-1,-1, 1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
-1,-1, 2,-1,-1, 3,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
-1, 2,-1, 2, 3,-1, 3,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
-1,-1, 2,-1,-1, 3,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
-1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,
};

const int8_t Board::RAD2TRAP[BSIZE] =
{
ESQ,ESQ, TRAP0,ESQ,ESQ, TRAP1,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ, TRAP0,ESQ, TRAP0, TRAP1,ESQ, TRAP1,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
 TRAP0,ESQ,ESQ, TRAP1, TRAP0,ESQ,ESQ, TRAP1,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ, TRAP0, TRAP2, TRAP0, TRAP1, TRAP3, TRAP1,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ, TRAP2, TRAP0, TRAP2, TRAP3, TRAP1, TRAP3,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
 TRAP2,ESQ,ESQ, TRAP3, TRAP2,ESQ,ESQ, TRAP3,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ, TRAP2,ESQ, TRAP2, TRAP3,ESQ, TRAP3,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
ESQ,ESQ, TRAP2,ESQ,ESQ, TRAP3,ESQ,ESQ,  ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,ESQ,
};

const int Board::STARTY[2] = {7,0};
const int Board::GOALY[2] = {0,7};
const int Board::GOALYINC[2] = {-1,+1};
const int Board::GOALLOCINC[2] = {S,N};

const bool Board::ISGOAL[2][BSIZE] =
{{
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
},{
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
}};

const Bitmap Board::PGOALMASKS[8][2] =
{
    {Bitmap(0x00000000000000FFull), Bitmap(0xFF00000000000000ull)},
    {Bitmap(0x000000000000FFFFull), Bitmap(0xFFFF000000000000ull)},
    {Bitmap(0x0000000000FFFFFFull), Bitmap(0xFFFFFF0000000000ull)},
    {Bitmap(0x00000000FFFFFFFFull), Bitmap(0xFFFFFFFF00000000ull)},
    {Bitmap(0x000000FFFFFFFFFFull), Bitmap(0xFFFFFFFFFF000000ull)},
    {Bitmap(0x0000FFFFFFFFFFFFull), Bitmap(0xFFFFFFFFFFFF0000ull)},
    {Bitmap(0x00FFFFFFFFFFFFFFull), Bitmap(0xFFFFFFFFFFFFFF00ull)},
    {Bitmap(0xFFFFFFFFFFFFFFFFull), Bitmap(0xFFFFFFFFFFFFFFFFull)},
};

const int Board::ADJOFFSETS[4] =
{S,W,E,N};

const int Board::STEPOFFSETS[4] =
{MS, MW, ME, MN};

const bool Board::ADJOKAY[4][BSIZE] =
{
{
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
},
{
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
},
{
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,false,   false,false,false,false,false,false,false,false,
},
{
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
true ,true ,true ,true ,true ,true ,true ,true ,   false,false,false,false,false,false,false,false,
false,false,false,false,false,false,false,false,   false,false,false,false,false,false,false,false,
},
};

const int Board::GOALYDIST[2][BSIZE] =
{{
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
2,2,2,2,2,2,2,2,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
5,5,5,5,5,5,5,5,  0,0,0,0,0,0,0,0,
6,6,6,6,6,6,6,6,  0,0,0,0,0,0,0,0,
7,7,7,7,7,7,7,7,  0,0,0,0,0,0,0,0,
},{
7,7,7,7,7,7,7,7,  0,0,0,0,0,0,0,0,
6,6,6,6,6,6,6,6,  0,0,0,0,0,0,0,0,
5,5,5,5,5,5,5,5,  0,0,0,0,0,0,0,0,
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
2,2,2,2,2,2,2,2,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
}};

const int Board::ADVANCEMENT[2][BSIZE] =
{{
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
2,2,2,2,2,2,2,2,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
},{
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,  0,0,0,0,0,0,0,0,
2,2,2,2,2,2,2,2,  0,0,0,0,0,0,0,0,
3,3,3,3,3,3,3,3,  0,0,0,0,0,0,0,0,
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
4,4,4,4,4,4,4,4,  0,0,0,0,0,0,0,0,
}};

const int Board::ADVANCE_OFFSET[2] = {S,N};

const int8_t Board::CLOSEST_TRAP[BSIZE] =
{
TRAP0,TRAP0,TRAP0,TRAP0,TRAP1,TRAP1,TRAP1,TRAP1,  0,0,0,0,0,0,0,0,
TRAP0,TRAP0,TRAP0,TRAP0,TRAP1,TRAP1,TRAP1,TRAP1,  0,0,0,0,0,0,0,0,
TRAP0,TRAP0,TRAP0,TRAP0,TRAP1,TRAP1,TRAP1,TRAP1,  0,0,0,0,0,0,0,0,
TRAP0,TRAP0,TRAP0,TRAP0,TRAP1,TRAP1,TRAP1,TRAP1,  0,0,0,0,0,0,0,0,
TRAP2,TRAP2,TRAP2,TRAP2,TRAP3,TRAP3,TRAP3,TRAP3,  0,0,0,0,0,0,0,0,
TRAP2,TRAP2,TRAP2,TRAP2,TRAP3,TRAP3,TRAP3,TRAP3,  0,0,0,0,0,0,0,0,
TRAP2,TRAP2,TRAP2,TRAP2,TRAP3,TRAP3,TRAP3,TRAP3,  0,0,0,0,0,0,0,0,
TRAP2,TRAP2,TRAP2,TRAP2,TRAP3,TRAP3,TRAP3,TRAP3,  0,0,0,0,0,0,0,0,
};

const int Board::CLOSEST_TRAPINDEX[BSIZE] =
{
0,0,0,0,1,1,1,1,  0,0,0,0,0,0,0,0,
0,0,0,0,1,1,1,1,  0,0,0,0,0,0,0,0,
0,0,0,0,1,1,1,1,  0,0,0,0,0,0,0,0,
0,0,0,0,1,1,1,1,  0,0,0,0,0,0,0,0,
2,2,2,2,3,3,3,3,  0,0,0,0,0,0,0,0,
2,2,2,2,3,3,3,3,  0,0,0,0,0,0,0,0,
2,2,2,2,3,3,3,3,  0,0,0,0,0,0,0,0,
2,2,2,2,3,3,3,3,  0,0,0,0,0,0,0,0,
};

const int Board::CLOSEST_TDIST[BSIZE] =
{
4,3,2,3,3,2,3,4,  0,0,0,0,0,0,0,0,
3,2,1,2,2,1,2,3,  0,0,0,0,0,0,0,0,
2,1,0,1,1,0,1,2,  0,0,0,0,0,0,0,0,
3,2,1,2,2,1,2,3,  0,0,0,0,0,0,0,0,
3,2,1,2,2,1,2,3,  0,0,0,0,0,0,0,0,
2,1,0,1,1,0,1,2,  0,0,0,0,0,0,0,0,
3,2,1,2,2,1,2,3,  0,0,0,0,0,0,0,0,
4,3,2,3,3,2,3,4,  0,0,0,0,0,0,0,0,
};

const int Board::CENTERDIST[BSIZE] =
{
6,5,4,3,3,4,5,6,  0,0,0,0,0,0,0,0,
5,4,3,2,2,3,4,5,  0,0,0,0,0,0,0,0,
4,3,2,1,1,2,3,4,  0,0,0,0,0,0,0,0,
3,2,1,0,0,1,2,3,  0,0,0,0,0,0,0,0,
3,2,1,0,0,1,2,3,  0,0,0,0,0,0,0,0,
4,3,2,1,1,2,3,4,  0,0,0,0,0,0,0,0,
5,4,3,2,2,3,4,5,  0,0,0,0,0,0,0,0,
6,5,4,3,3,4,5,6,  0,0,0,0,0,0,0,0,
};


const int8_t Board::SYMLOC[2][BSIZE] =
{
{
28,29,30,31,31,30,29,28,  0,0,0,0,0,0,0,0,
24,25,26,27,27,26,25,24,  0,0,0,0,0,0,0,0,
20,21,22,23,23,22,21,20,  0,0,0,0,0,0,0,0,
16,17,18,19,19,18,17,16,  0,0,0,0,0,0,0,0,
12,13,14,15,15,14,13,12,  0,0,0,0,0,0,0,0,
 8, 9,10,11,11,10, 9, 8,  0,0,0,0,0,0,0,0,
 4, 5, 6, 7, 7, 6, 5, 4,  0,0,0,0,0,0,0,0,
 0, 1, 2, 3, 3, 2, 1, 0,  0,0,0,0,0,0,0,0,
}
,
{
 0, 1, 2, 3, 3, 2, 1, 0,  0,0,0,0,0,0,0,0,
 4, 5, 6, 7, 7, 6, 5, 4,  0,0,0,0,0,0,0,0,
 8, 9,10,11,11,10, 9, 8,  0,0,0,0,0,0,0,0,
12,13,14,15,15,14,13,12,  0,0,0,0,0,0,0,0,
16,17,18,19,19,18,17,16,  0,0,0,0,0,0,0,0,
20,21,22,23,23,22,21,20,  0,0,0,0,0,0,0,0,
24,25,26,27,27,26,25,24,  0,0,0,0,0,0,0,0,
28,29,30,31,31,30,29,28,  0,0,0,0,0,0,0,0,
}
};

const int8_t Board::PSYMLOC[2][BSIZE] =
{
{
56,57,58,59,60,61,62,63,  0,0,0,0,0,0,0,0,
48,49,50,51,52,53,54,55,  0,0,0,0,0,0,0,0,
40,41,42,43,44,45,46,47,  0,0,0,0,0,0,0,0,
32,33,34,35,36,37,38,39,  0,0,0,0,0,0,0,0,
24,25,26,27,28,29,30,31,  0,0,0,0,0,0,0,0,
16,17,18,19,20,21,22,23,  0,0,0,0,0,0,0,0,
 8, 9,10,11,12,13,14,15,  0,0,0,0,0,0,0,0,
 0, 1, 2, 3, 4, 5, 6, 7,  0,0,0,0,0,0,0,0,
}
,
{
 0, 1, 2, 3, 4, 5, 6, 7,  0,0,0,0,0,0,0,0,
 8, 9,10,11,12,13,14,15,  0,0,0,0,0,0,0,0,
16,17,18,19,20,21,22,23,  0,0,0,0,0,0,0,0,
24,25,26,27,28,29,30,31,  0,0,0,0,0,0,0,0,
32,33,34,35,36,37,38,39,  0,0,0,0,0,0,0,0,
40,41,42,43,44,45,46,47,  0,0,0,0,0,0,0,0,
48,49,50,51,52,53,54,55,  0,0,0,0,0,0,0,0,
56,57,58,59,60,61,62,63,  0,0,0,0,0,0,0,0,
}
};

const int Board::SYMTRAPINDEX[2][4] =
{{2,3,0,1},{0,1,2,3}};

const int Board::SYMDIR[2][4] =
{{1,2,2,0},{0,2,2,1}};

const int Board::PSYMDIR[2][4] =
{{3,1,2,0},{0,1,2,3}};

//INITIALIZATION OF PRECOMPUTED TABLES-----------------------------------------------

static bool isInBounds(int x, int y)
{
  return x >= 0 && x < 8 && y >= 0 && y < 8;
}

static Bitmap makeGPruneMask(pla_t pla, loc_t loc, int gDist)
{
  int dy = pla*2-1;
  int yGoal = pla*7;
  int yDist = (pla == 0) ? gY(loc) : 7-gY(loc);
  int x = gX(loc);
  Bitmap b;
  if(yDist == 0)
  {
    b.setOn(loc);
    return b;
  }

  for(int i = 1; i<=yDist; i++)
    b.setOn(gLoc(x,yGoal-i*dy));

  int reps = (gDist-yDist)*2 + 1;
  for(int i = 0; i<reps; i++)
    b |= Bitmap::adj(b);

  return b;
}

void Board::initData()
{
  Rand rand(Hash::simpleHash("Board::initData"));

  //Initialize zobrist hash values for pieces at locs
  hash_t r = 0LL;
  for(pla_t i = 0; i<2; i++)
  {
    for(loc_t k = 0; k<BSIZE; k ++)
      HASHPIECE[i][0][k] = 0LL;

    for(piece_t j = 1; j<NUMTYPES; j++)
    {
      for(loc_t k = 0; k<BSIZE; k++)
      {
        if(k & 0x88)
          continue;
        r = 0LL;
        while(r == 0LL)
          r = rand.nextUInt64();
        HASHPIECE[i][j][k] = r;
      }
    }
  }

  //Initialize zobrist hash value for player
  for(pla_t i = 0; i<2; i++)
  {
    r = 0LL;
    while(r == 0LL)
      r = rand.nextUInt64();
    HASHPLA[i] = r;
  }

  //Initialize zobrist hash value for setp
  for(int i = 0; i<4; i++)
  {
    r = 0LL;
    while(r == 0LL)
      r = rand.nextUInt64();
    HASHSTEP[i] = r;
  }

  //Initialize goal masks
  GOALMASKS[0] = Bitmap::BMPY[0];
  GOALMASKS[1] = Bitmap::BMPY[7];

  for(loc_t k0 = 0; k0 < BSIZE; k0++)
  {
    if(k0 & 0x88)
      continue;
    for(loc_t k1 = 0; k1 < BSIZE; k1++)
    {
      if(k1 & 0x88)
        continue;
      int x0 = gX(k0);
      int y0 = gY(k0);
      int x1 = gX(k1);
      int y1 = gY(k1);

      //Initialize manhattan distance for locations
      MANHATTANDIST[k0 - k1 + DIFFIDX_ADD] = abs(x0-x1) + abs(y0-y1);

      //Initialize adjacency matrix for locations
      if((CS1(k0) && k0 S == k1) ||
         (CW1(k0) && k0 W == k1) ||
         (CE1(k0) && k0 E == k1) ||
         (CN1(k0) && k0 N == k1))
      {
        ISADJACENT[k0 - k1 + DIFFIDX_ADD] = true;
        ISADJACENT[k1 - k0 + DIFFIDX_ADD] = true;
      }
      else
      {
        ISADJACENT[k0 - k1 + DIFFIDX_ADD] = false;
        ISADJACENT[k1 - k0 + DIFFIDX_ADD] = false;
      }

      //Initialize direction to step to be closer
      if(k1 == k0)
      {
        DIRTORFAR[k0 - k1 + DIFFIDX_ADD] = 0;
        DIRTORNEAR[k0 - k1 + DIFFIDX_ADD] = 0;
      }
      else
      {
        int dx = abs(x1-x0);
        int dy = abs(y1-y0);
        int xDir = x1 > x0 ? 2 : 1;
        int yDir = y1 > y0 ? 3 : 0;
        if(dx == 0)
        {
          DIRTORFAR[k0 - k1 + DIFFIDX_ADD] = yDir;
          DIRTORNEAR[k0 - k1 + DIFFIDX_ADD] = yDir;
        }
        else if(dy == 0)
        {
          DIRTORFAR[k0 - k1 + DIFFIDX_ADD] = xDir;
          DIRTORNEAR[k0 - k1 + DIFFIDX_ADD] = xDir;
        }
        else if(dx >= dy)
        {
          DIRTORFAR[k0 - k1 + DIFFIDX_ADD] = xDir;
          DIRTORNEAR[k0 - k1 + DIFFIDX_ADD] = yDir;
        }
        else// if(dx < dy)
        {
          DIRTORFAR[k0 - k1 + DIFFIDX_ADD] = yDir;
          DIRTORNEAR[k0 - k1 + DIFFIDX_ADD] = xDir;
        }
      }
    }
  }

  //Initialize spiral listing of locations
  for(loc_t k0 = 0; k0 < BSIZE; k0++)
  {
    if(k0 & 0x88)
      continue;
    int x0 = gX(k0);
    int y0 = gY(k0);

    int index = 0;
    int diagLen = 1;
    while(index < 64)
    {
      for(int dx = 0; dx < diagLen; dx++)
      {
        int dy = diagLen-1-dx;
        if(isInBounds(x0+dx,y0+dy))                       {SPIRAL[k0][index++] = gLoc(x0+dx,y0+dy);}
        if(isInBounds(x0-dx,y0+dy) && dx != 0)            {SPIRAL[k0][index++] = gLoc(x0-dx,y0+dy);}
        if(isInBounds(x0+dx,y0-dy) && dy != 0)            {SPIRAL[k0][index++] = gLoc(x0+dx,y0-dy);}
        if(isInBounds(x0-dx,y0-dy) && dx != 0 && dy != 0) {SPIRAL[k0][index++] = gLoc(x0-dx,y0-dy);}
      }
      diagLen++;
    }
  }

  //Initialize radius and disk bitmaps
  for(loc_t k0 = 0; k0 < BSIZE; k0++)
  {
    if(k0 & 0x88)
      continue;
    Bitmap b;
    b.setOn(k0);
    RADIUS[0][k0] = b;
    DISK[0][k0] = b;

    for(int i = 1; i<16; i++)
    {
      DISK[i][k0] = DISK[i-1][k0] | Bitmap::adj(DISK[i-1][k0]);
      RADIUS[i][k0] = DISK[i][k0] & (~DISK[i-1][k0]);
    }
  }

  //Initialize goal pruning masks
  for(pla_t pla = 0; pla <= 1; pla++)
    for(loc_t loc = 0; loc < BSIZE; loc++)
    {
      if(loc & 0x88)
        continue;
      for(int gDist = 0; gDist < 5; gDist++)
        GPRUNEMASKS[pla][loc][gDist] = makeGPruneMask(pla,loc,gDist);
    }

  //Initialize interval maps
  for(int start = 0; start<8; start++)
  {
    for(int end = 0; end<8; end++)
    {
      Bitmap xMap;
      Bitmap yMap;
      int min = start < end ? start : end;
      int max = start < end ? end : start;
      for(int i = min; i<= max; i++)
        xMap |= Bitmap::BMPX[i];
      for(int i = min; i<= max; i++)
        yMap |= Bitmap::BMPY[i];
      XINTERVAL[start][end] = xMap;
      YINTERVAL[start][end] = yMap;
    }
  }
}

