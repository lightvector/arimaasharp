
/*
 * setup.cpp
 * Author: davidwu
 */

#include <cctype>
#include <cmath>
#include "../core/global.h"
#include "../core/hash.h"
#include "../core/rand.h"
#include "../board/bitmap.h"
#include "../board/board.h"
#include "../setups/setup.h"

using namespace std;

//Take a setup of 16 pieces and place them all for the current player
static void place(Board& b, string setup);

//Permute the characters in the string
static string permute(string setup, Rand& rand);

//Reverse the given setup of 16
static string reverse(string setup);

//Get the strength of the piece for a given lowercase char
static int getPieceStrength(char c);

//FORWARD DECLARATIONS--------------------------------------------------------
static string getSetupNormalString(const Board& b, uint64_t seed);
static string getRatedRandomSetupString(const Board& b, uint64_t seed);
static string getPartialRandomSetupString(const Board& b, uint64_t seed);
static string getRandomSetupString(const Board& b, uint64_t seed);

void Setup::testSetupVariability()
{
  Board b;
  map<string,int> stringmap;
  for(int i = 0; i<10000; i++)
  {
    if(i%1000 == 0)
      cout << i << " " << stringmap.size() << endl;
    string s = getPartialRandomSetupString(b,i);
    stringmap[s] += 1;
  }
  cout << stringmap.size() << endl;

  int max = 0;
  for(map<string,int>::iterator it = stringmap.begin(); it != stringmap.end(); ++it)
  {
    if(max < (*it).second)
      max = (*it).second;
  }
  cout << max << endl;
}



void Setup::setup(Board& b, int type, uint64_t seed)
{
  if(type == SETUP_NORMAL)
    setupNormal(b,seed);
  else if(type == SETUP_RATED_RANDOM)
    setupRatedRandom(b,seed);
  else if(type == SETUP_PARTIAL_RANDOM)
    setupPartialRandom(b,seed);
  else if(type == SETUP_RANDOM)
    setupRandom(b,seed);
  else
    Global::fatalError("Unknown setup type");
}

void Setup::setupNormal(Board& b, uint64_t seed)
{
  place(b,getSetupNormalString(b,seed));
}

void Setup::setupRatedRandom(Board& b, uint64_t seed)
{
  place(b,getRatedRandomSetupString(b,seed));
}

void Setup::setupPartialRandom(Board& b, uint64_t seed)
{
  place(b,getPartialRandomSetupString(b,seed));
}

void Setup::setupRandom(Board& b, uint64_t seed)
{
  place(b,getRandomSetupString(b,seed));
}

//BASIC GOOD SETUP---------------------------------------------------------

static const int numRabbitSetups = 12;
static const int rabbitSetupFreq[numRabbitSetups] =
{
  3,  //0
  1,  //1
  1,  //2
  1,  //3
  1,  //4
  1,  //5
  1,  //6
  1,  //7
  1,  //8
  1,  //9
  1,  //10
  1,  //11
};
static const char* rabbitSetups[numRabbitSetups] =
{
  //0
  "r......r"
  "rrr..rrr",
  //1
  "r......r"
  "r.rr.rrr",
  //2
  "r......r"
  "rrr.rr.r",
  //3
  ".......r"
  "rrrr.rrr",
  //4
  "r......."
  "rrr.rrrr",
  //5
  ".......r"
  "rrr.rrrr",
  //6
  "r......."
  "rrrr.rrr",
  //7
  "r.r....r"
  "r.r..rrr",
  //8
  "r.r....r"
  "r.r.rr.r",
  //9
  "r....r.r"
  "rrr..r.r",
  //10
  "r....r.r"
  "r.rr.r.r",
  //11
  "r.r..r.r"
  "r.r..r.r",
};


static const int ePosFreq[8] =
{0,0,0,8,8,0,0,0};
static const int camelHorsePos[16] =
{1,6,3,4,2,5,11,12,0,7,9,14,10,13,8,15};

static const int numDogCatPerms = 4;
static const char* dogCatPerm[numDogCatPerms] =
{"dccd","cddc", "dcdc", "cdcd",};

static string getSetupNormalString(const Board& b, uint64_t seed)
{
  Rand rand(seed + Hash::simpleHash("Setup::getSetupNormalString"));

  string setup = rabbitSetups[rand.nextUInt(rabbitSetupFreq,numRabbitSetups)];

  //Position elephant
  int epFreq[8];
  for(int i = 0; i<8; i++)
    epFreq[i] = (setup[i] == '.') ? ePosFreq[i] : 0;
  setup[rand.nextUInt(epFreq,8)] = 'e';

  //Position camel and horses
  int goodPos[3];
  int gP = 0;
  int iP = 0;
  while(gP < 3)
  {
    if(setup[camelHorsePos[iP]] == '.')
      goodPos[gP++] = camelHorsePos[iP];
    iP++;
  }
  for(int i = 0; i<3; i++)
    setup[goodPos[i]] = 'h';
  setup[goodPos[rand.nextUInt(3)]] = 'm';

  //Position dogs and cats
  string dcPerm = dogCatPerm[rand.nextUInt(numDogCatPerms)];
  int index = 0;
  for(int x = 0; x<8; x++)
  {
    if(setup[x] == '.')
      setup[x] = dcPerm[index++];
    if(setup[x+8] == '.')
      setup[x+8] = dcPerm[index++];
  }

  //Reverse
  if(rand.nextUInt(2) == 0)
    setup = reverse(setup);

  //Reverse
  if(b.player == 0 && b.pieces[gLoc(8+setup.find('e',0))] == ELE)
    setup = reverse(setup);

  return setup;
}


//RATEDPARTALRANDOM SETUP-------------------------------------------------------
static string getRatedRandomSetupString(const Board& b, uint64_t seed)
{
  Rand rand(seed + Hash::simpleHash("Setup::getRatedRandomSetupString"));

  string bestSetup = getRandomSetupString(b, seed);
  int bestRating = -1;
  for(int i = 0; i<8; i++)
  {
    string setup = getPartialRandomSetupString(b, seed);

    //Reverse
    if(b.player == 0 && b.pieces[gLoc(8+setup.find('e',0))] == ELE)
      setup = reverse(setup);

    int rating = 0;

    //Elephant should be in the middle
    int epos = setup.find('e',0);
    if(epos == 3 || epos == 4)
      rating += 7;
    if(epos == 2 || epos == 5)
      rating += 4;

    //Horses should not be in the back row
    int hpos = setup.find('h',0);
    if(hpos <= 7)
      rating += 2;
    int hpos2 = setup.find('h',hpos);
    if(hpos2 <= 7)
      rating += 2;

    //Sides of traps should be strong
    if(setup[1] == 'e' || setup[1] == 'm' || setup[1] == 'h')
      rating += 4;
    if(setup[6] == 'e' || setup[6] == 'm' || setup[6] == 'h')
      rating += 4;

    //Add a little noise anyways
    rating += rand.nextInt(0,3);

    if(rating > bestRating)
    {
      bestRating = rating;
      bestSetup = setup;
    }
  }
  return bestSetup;
}

//PARTALRANDOM SETUP-------------------------------------------------------

static string getPartialRandomSetupString(const Board& b, uint64_t seed)
{
  (void)b; //unused
  Rand rand(seed + Hash::simpleHash("Setup::getPartialRandomSetupString"));

  string setup = string("emhhddccrrrrrrrr");
  setup = permute(setup, rand);

  //Permute until we have a good base setup, try 200 times
  int tries = 0;
  while(tries < 200)
  {
    tries++;
    setup = permute(setup,rand);

    //Make sure corners have rabbits
    if(setup[8] != 'r' || setup[15] != 'r')
      continue;

    //Make sure elephant not at edge
    if(setup[0] == 'e' || setup[7] == 'e' || setup[9] == 'e' || setup[14] == 'e')
      continue;

    //Make sure no double rabbit column in center
    if(setup[3] == 'r' && setup[11] == 'r')
      continue;
    if(setup[4] == 'r' && setup[12] == 'r')
      continue;

    break;
  }

  //Make sure no strong piece is behind a weaker one
  for(int i = 0; i<8; i++)
  {
    if(getPieceStrength(setup[i]) < getPieceStrength(setup[i+8]))
    {
      char temp = setup[i];
      setup[i] = setup[i+8];
      setup[i+8] = temp;
    }
  }

  //Done!
  return setup;
}

//TRUE RANDOM SETUP-------------------------------------------------

static string getRandomSetupString(const Board& b, uint64_t seed)
{
  (void)b; //unused
  Rand rand(seed + Hash::simpleHash("Setup::getRandomSetupString"));

  string setup = string("emhhddccrrrrrrrr");
  setup = permute(setup,rand);
  return setup;
}


//HELPERS ------------------------------------------------------------------
static int getPieceStrength(char c)
{
  switch(c)
  {
  case 'e': return 6;
  case 'm': return 5;
  case 'h': return 4;
  case 'd': return 3;
  case 'c': return 2;
  case 'r': return 1;
  default: DEBUGASSERT(false); break;
  }
  return 0;
}

static string reverse(string setup)
{
  string rSetup = setup;
  for(int x = 0; x<8; x++)
  {
    rSetup[x] = setup[7-x];
    rSetup[x+8] = setup[7-x+8];
  }
  return rSetup;
}

static string permute(string setup, Rand& rand)
{
  int size = setup.size();
  for(int i = 1; i<size; i++)
  {
    int j = rand.nextUInt(i+1);
    char temp = setup[j];
    setup[j] = setup[i];
    setup[i] = temp;
  }
  return setup;
}

static void place(Board& b, string setup)
{
  if(setup.size() != 16)
    Global::fatalError("Setup, place given string of length != 16");

  pla_t pla = b.player;
  if(pla == 1)
  {
    for(int x = 0; x<8; x++)
    {
      char c = toupper(setup[x+8]);
      b.setPiece(gLoc(x,0),Board::readOwner(c),Board::readPiece(c));
      c = toupper(setup[x]);
      b.setPiece(gLoc(x,1),Board::readOwner(c),Board::readPiece(c));
    }
    b.setPlaStep(0,0);
  }
  else if(pla == 0)
  {
    for(int x = 0; x<8; x++)
    {
      char c = tolower(setup[x]);
      b.setPiece(gLoc(x,6),Board::readOwner(c),Board::readPiece(c));
      c = tolower(setup[x+8]);
      b.setPiece(gLoc(x,7),Board::readOwner(c),Board::readPiece(c));
    }
    b.setPlaStep(1,0);
  }
  b.refreshStartHash();
}






