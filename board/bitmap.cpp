
#include "bitmap.h"

const Bitmap Bitmap::BMPZEROS = Bitmap();
const Bitmap Bitmap::BMPONES = Bitmap(0xFFFFFFFFFFFFFFFFULL);
const Bitmap Bitmap::BMPTRAPS = Bitmap(0x0000240000240000ULL);
const Bitmap Bitmap::BMPBEHINDTRAPS = Bitmap(0x0024000000002400ULL);

//TODO why are these not used more often? Particularly BMPY[0] and BMPY[7] for goals?
const Bitmap Bitmap::BMPX[8] = {
Bitmap(0x0101010101010101ULL),
Bitmap(0x0202020202020202ULL),
Bitmap(0x0404040404040404ULL),
Bitmap(0x0808080808080808ULL),
Bitmap(0x1010101010101010ULL),
Bitmap(0x2020202020202020ULL),
Bitmap(0x4040404040404040ULL),
Bitmap(0x8080808080808080ULL),
};

const Bitmap Bitmap::BMPY[8] = {
Bitmap(0x00000000000000FFULL),
Bitmap(0x000000000000FF00ULL),
Bitmap(0x0000000000FF0000ULL),
Bitmap(0x00000000FF000000ULL),
Bitmap(0x000000FF00000000ULL),
Bitmap(0x0000FF0000000000ULL),
Bitmap(0x00FF000000000000ULL),
Bitmap(0xFF00000000000000ULL),
};