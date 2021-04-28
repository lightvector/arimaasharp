
/*
 * gamerecord.cpp
 * Author: davidwu
 */

#include "../core/global.h"
#include "../board/board.h"
#include "../board/gamerecord.h"

using namespace std;

BoardRecord::BoardRecord()
:board(),keyValues()
{

}

BoardRecord::BoardRecord(const Board& b, const map<string,string>& k)
:board(b),keyValues(k)
{

}

GameRecord::GameRecord()
:board(),moves(),winner(NPLA),keyValues()
{

}

GameRecord::GameRecord(const Board& b, const vector<move_t>& m, pla_t w, const map<string,string>& k)
:board(b),moves(m),winner(w),keyValues(k)
{

}



