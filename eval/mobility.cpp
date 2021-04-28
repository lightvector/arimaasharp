/*
 * mobility.cpp
 * Author: davidwu
 */
#include "../core/global.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../eval/internal.h"

void Mobility::getEle(const Board& b, pla_t pla, Bitmap* eleCanReachInN, int max, int len)
{
  DEBUGASSERT(len >= max + Mobility::EXTRA_LEN);
  pla_t opp = gOpp(pla);
  Bitmap plaEle = b.pieceMaps[pla][ELE];
  Bitmap oppEle = b.pieceMaps[opp][ELE];

  //All of the maps below treat it as if plaEle weren't on the board
  Bitmap plas = b.pieceMaps[pla][0] & ~plaEle;
  Bitmap opps = b.pieceMaps[opp][0];
  Bitmap oppNonEle = opps & ~oppEle;
  Bitmap plaRab = b.pieceMaps[pla][RAB];
  Bitmap plaNonRab = plas & ~plaRab;
  Bitmap empty = ~(plas | opps);
  Bitmap openF = Bitmap::shiftG(empty,pla);
  Bitmap openG = Bitmap::shiftF(empty,pla);
  Bitmap openW = Bitmap::shiftE(empty);
  Bitmap openE = Bitmap::shiftW(empty);

  Bitmap open = openF | openG | openW | openE;
  Bitmap open2 = Bitmap::adj2(empty);
  Bitmap openWEG2 = (openG & (openW | openE)) | (openW & openE);
  Bitmap plaOpenToStep2 = (plaNonRab & open2) | (plaRab & openWEG2);
  Bitmap open3 = ~Bitmap::adj2(~empty);
  Bitmap plaUFRad2 = Bitmap::adj(Bitmap::adj(plas & ~b.frozenMap));

  Bitmap trapSafe = ~Bitmap::BMPTRAPS | Bitmap::adj(plas);
  Bitmap trapSafe2 = ~Bitmap::BMPTRAPS | Bitmap::adj2(plas);
  Bitmap adjUnTrapSafe2 = Bitmap::adj(~trapSafe2);

  for(int i = 0; i<max; i++)
    eleCanReachInN[i] = plaEle;

  for(int i = 0; i<max-1; i++)
  {
    Bitmap map = eleCanReachInN[i];

    //We can't step into a emp/pla square if we're coming from  a square with lots of stuff or that was blocked earlier
    //adjUnTrapSafe2 - we can't step into a square if it would leave another piece hanging nearby
    //trapSafe2 for plas - we can't step into a pla square if moving the pla would sac the ele
    Bitmap comingFreelyForPlas = Bitmap::adj(map & (empty | (open3 & (~plaRab | openG))) & trapSafe2 & ~adjUnTrapSafe2);
    Bitmap comingFreelyForEmpty = Bitmap::adj(map & (empty | (open3 & (~plaRab | openG))) & ~adjUnTrapSafe2);
    Bitmap comingFreelyForOpp = Bitmap::adj(map & ~adjUnTrapSafe2);
    Bitmap comingFromStuff = Bitmap::adj(map & ~empty);
    Bitmap coming = Bitmap::adj(map);

    //Various conditions under which we can reach a square in some number of steps from a square we could reach in fewer

    //EMPTY
    eleCanReachInN[i+1] |= map;
    eleCanReachInN[i+1] |= comingFreelyForEmpty & empty & trapSafe;
    eleCanReachInN[i+2] |= coming & empty & trapSafe;
    eleCanReachInN[i+2] |= comingFreelyForEmpty & empty & open3 & plaUFRad2;
    eleCanReachInN[i+3] |= coming & empty & open3 & plaUFRad2;

    //PLA
    eleCanReachInN[i+2] |= comingFreelyForPlas & plaOpenToStep2;
    eleCanReachInN[i+3] |= comingFreelyForPlas & plaNonRab;

    //OPP
    eleCanReachInN[i+2] |= comingFreelyForOpp & oppNonEle & open2 & trapSafe;
    eleCanReachInN[i+2] |= comingFreelyForOpp & comingFromStuff & oppNonEle & open & trapSafe;
    eleCanReachInN[i+3] |= coming & oppNonEle & open2 & trapSafe;
    eleCanReachInN[i+3] |= comingFromStuff & oppNonEle & open & trapSafe;
  }
}

static const Bitmap BMPMIDDLE16 = Bitmap(0x00003C3C3C3C0000);
static const Bitmap BMPFORWARD30[2] = {
    Bitmap(0x00007E7E7E7E7E00),
    Bitmap(0x007E7E7E7E7E0000),
};
static const Bitmap BMPFORWARD12[2] = {
    Bitmap(0x0000003C3C3C0000),
    Bitmap(0x00003C3C3C000000),
};

//40+ is quite free and normal
//30s are a little low, ele starting to be decentralized
//20s are low, ele decentralized or stuffy
//10s and are very low
//less than 10 is basically blockaded
static const int MOBILITY_PENALTY_LEN = 60;
static const int MOBILITY_PENALTY[MOBILITY_PENALTY_LEN] = {
430,420,410,400,390,380,370,358,345,330,
313,295,275,255,235,215,195,174,157,140,
126,113,101, 90, 80, 72, 64, 58, 52, 46,
 40, 34, 29, 26, 22, 19, 16, 13, 11,  9,
  7,  6,  4,  4,  3,  3,  2,  2,  1,  1,
  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
};

eval_t Mobility::getEval(const Board& b, const Bitmap eleCanReach[EMOB_ARR_LEN], pla_t pla)
{
  //We reward blocking the opponent slightly if you can use his own pieces - opp pieces adjacent
  //are counted when summing up mobility
  Bitmap reach4 = eleCanReach[4] | (Bitmap::adj(eleCanReach[0]) & b.pieceMaps[gOpp(pla)][0]);

  int mobility = 0;
  mobility += reach4.countBits();
  mobility += (reach4 & BMPFORWARD30[pla]).countBits();
  mobility += (reach4 & BMPFORWARD12[pla]).countBits();
  mobility += (eleCanReach[3] & BMPMIDDLE16).countBits();

  if(mobility >= MOBILITY_PENALTY_LEN)
    return 0;
  return -MOBILITY_PENALTY[mobility];
}

void Mobility::print(const Board& b, const Bitmap* eleCanReach, pla_t pla, int max, ostream& out)
{
  for(int y = 7; y >= 0; y--)
  {
    for(int x = 0; x < 8; x++)
    {
      loc_t loc = gLoc(x,y);
      int i = 0;
      while(i<max && !eleCanReach[i].isOne(loc)) i++;
      if(i<10) out << i;
      else out << (char)('A'+i-10);
    }
    out << endl;
  }

  Bitmap reach4 = eleCanReach[4] | (Bitmap::adj(eleCanReach[0]) & b.pieceMaps[gOpp(pla)][0]);
  int mobility = 0;
  mobility += reach4.countBits();
  mobility += (reach4 & BMPFORWARD30[pla]).countBits();
  mobility += (reach4 & BMPFORWARD12[pla]).countBits();
  mobility += (eleCanReach[3] & BMPMIDDLE16).countBits();
  out << "Mobility " << mobility << endl;
}


