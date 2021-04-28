
/*
 * boardtreeconst.h
 * Author: David Wu
 *
 * Useful macros for the capture and defense trees
 */

#ifndef BOARDTREECONST_H_
#define BOARDTREECONST_H_

#include "../board/btypes.h"

#define HMVALUE(x) (x)
#define TS(x,y,a) b.tempStep((x),(y)); a; b.tempStep((y),(x));
#define TP(x,y,z,a) b.tempPP((x),(y),(z)); a; b.tempPP((z),(y),(x));
#define TSC(x,y,a) TempRecord xtmprc = b.tempStepC((x),(y)); a; b.undoTemp(xtmprc);
#define TSC2(x,y,a) TempRecord xtmprc2 = b.tempStepC((x),(y)); a; b.undoTemp(xtmprc2);
#define TPC(x,y,z,a) TempRecord xtmprc1 = b.tempStepC((y),(z)); TempRecord xtmprc2 = b.tempStepC((x),(y)); a; b.undoTemp(xtmprc2); b.undoTemp(xtmprc1);
#define TR(x,a) char xtmpow = b.owners[(x)]; char xtmppi = b.pieces[(x)]; b.owners[(x)] = NPLA; b.pieces[(x)] = EMP; a; b.owners[(x)] = xtmpow; b.pieces[(x)] = xtmppi;

#define ISE(x) (b.owners[(x)] == NPLA)
#define ISP(x) (b.owners[(x)] == pla)
#define ISO(x) (b.owners[(x)] == opp)

#define GT(x,y) (b.pieces[(x)] > b.pieces[(y)])
#define GEQ(x,y) (b.pieces[(x)] >= b.pieces[(y)])
#define LEQ(x,y) (b.pieces[(x)] <= b.pieces[(y)])
#define LT(x,y) (b.pieces[(x)] < b.pieces[(y)])

#define GTP(x,y) (b.pieces[(x)] > (y))
#define GEQP(x,y) (b.pieces[(x)] >= (y))
#define LEQP(x,y) (b.pieces[(x)] <= (y))
#define LTP(x,y) (b.pieces[(x)] < (y))

#define ADDMOVEPP(m,h) *(mv++) = m; *(hm++) = h; num++;
#define ADDMOVE(m,h) mv[num] = m; hm[num] = h; num++;

#define RIF(x) if((x)) {return true;}

#define SETGM(x) b.goalTreeMove = getMove x;
#define PREGM(x) b.goalTreeMove = preConcatMove x;
#define PSTGM(x) b.goalTreeMove = postConcatMove x;

#define RIFGM(x,y) if((x)) {b.goalTreeMove = getMove y; return true;}
#define RIFPREGM(x,y) if((x)) {b.goalTreeMove = preConcatMove y; return true;}

#define F -dy
#define G +dy
#define FW -1-dy
#define FE +1-dy
#define GW -1+dy
#define GE +1+dy
#define FF -dy-dy
#define GG +dy+dy
#define FWW -2-dy
#define FEE +2-dy
#define GWW -2+dy
#define GEE +2+dy
#define FFW -1-dy-dy
#define FFE +1-dy-dy
#define FWWW -3-dy
#define FEEE +3-dy
#define FFWW -2-dy-dy
#define FFEE +2-dy-dy
#define FFFW -1-dy-dy-dy
#define FFFE +1-dy-dy-dy
#define GGW -1+dy+dy
#define GGE +1+dy+dy
#define FFF -dy-dy-dy
#define GGG +dy+dy+dy
#define GWWW -3+dy
#define GEEE +3+dy
#define GGGW -1+dy+dy+dy
#define GGGE +1+dy+dy+dy
#define GGGG +dy+dy+dy+dy

#define MG +DYDIR[pla]
#define MF +DYDIR[1-pla]


#endif
