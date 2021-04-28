
/*
 * global.h
 * Author: davidwu
 *
 * Various generic useful things used throughout the program.
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <map>
#include <vector>
#include <iostream>
#include <string>
#include <stdint.h>
#include <cassert>
using namespace std;

//GLOBAL DEFINES AND FLAGS----------------------------------------------------
#define ONLYINDEBUG(x) x
//#define ONLYINDEBUG(x) ((void)0);

//#define BLARG(x) if(!(x)) {Global::fatalError("ohno");}
//#define DEBUGASSERT(x) ONLYINDEBUG(BLARG(x);)
#define DEBUGASSERT(x) ONLYINDEBUG(assert(x);)

#define NORETURN __attribute__ ((noreturn))
#define PUREFUNC __attribute__ ((pure))

//Do some extra checks for out of bounds values
//#define EXTRA_BOUND_CHECKS

//Enable multithreading
//#define MULTITHREADING
#define MULTITHREADING_STD
//#define BOOST_USE_WINDOWS_H

//For buffering to avoid false sharing between threads, in bytes
#define CACHE_LINE_BUFFER_SIZE 64

//Use popcount instruction for bitmap. NOTE: This also requires the -msse4.2 flag
//and a 64 bit mode compile. Use objdump to verify.
//#define DO_USE_POPCOUNT

//GLOBAL FUNCTIONS------------------------------------------------------------
namespace Global
{
  //ERRORS----------------------------------

  //Report fatal error message and exit
  void fatalError(const char* s) NORETURN;
  void fatalError(const string& s) NORETURN;

  //TIME------------------------------------

  //Get string describing the current date, suitable for filenames
  string getTimeString();

  //STRINGS---------------------------------

  //To string conversions
  string charToString(char c);
  string intToString(int x);
  string doubleToString(double x);
  string int64ToString(int64_t x);
  string uint64ToHexString(uint64_t x);

  //String to conversions using the standard library parsing
  int stringToInt(const string& str);
  uint64_t stringToUInt64(const string& str);
  double stringToDouble(const string& str);
  bool stringToBool(const string& str);
  bool tryStringToInt(const string& str, int& x);
  bool tryStringToUInt64(const string& str, uint64_t& x);
  bool tryStringToDouble(const string& str, double& x);
  bool tryStringToBool(const string& str, bool& x);

  //Check if string is all whitespace
  bool isWhitespace(char c);
  bool isWhitespace(const string& s);

  //Trim whitespace off both ends of string
  string trim(const string& s);

  //Join strings with a delimiter between each one, from [start,end)
  string concat(const char* const* strs, size_t len, const char* delim);
  string concat(const vector<string>& strs, const char* delim);
  string concat(const vector<string>& strs, const char* delim, size_t start, size_t end);

  //Split string into tokens, trimming off whitespace
  vector<string> split(const string& s);
  //Split string based on the given delim, no trimming
  vector<string> split(const string& s, char delim);

  //Convert to upper or lower case
  string toUpper(const string& s);
  string toLower(const string& s);

  //Like sprintf, but returns a string
  string strprintf(const char* fmt, ...);

  //Check if a string consists entirely of digits, and parse the integer, checking for overflow
  bool isDigits(const string& str);
  bool isDigits(const string& str, int start, int end);
  int parseDigits(const string& str);
  int parseDigits(const string& str, int start, int end);

  //Character properties
  bool isDigit(char c);
  bool isAlpha(char c);

  //Check if character is in the given list
  bool strContains(const char* str, char c);
  //Check if every char in the string is in the allowed list
  bool stringCharsAllAllowed(const string& str, const char* allowedChars);

  //IO-------------------------------------

  //Read entire file whole
  string readFile(const char* filename);
  string readFile(const string& filename);

  //Read file into separate lines, using the specified delimiter character(s).
  //The delimiter characters are NOT included.
  vector<string> readFileLines(const char* filename, char delimiter = ' ');
  vector<string> readFileLines(const string& filename, char delimiter = ' ');

  //USER IO----------------------------

  //Display a message and ask the user to press a key to continue
  void pauseForKey();
}

//Named pairs and triples of data values
#define STRUCT_NAMED_SINGLE(A,B,C) struct C {A B; inline C(): B() {} inline C(A s_n_p_arg_0): B(s_n_p_arg_0) {}}
#define STRUCT_NAMED_PAIR(A,B,C,D,E) struct E {A B; C D; inline E(): B(),D() {} inline E(A s_n_p_arg_0, C s_n_p_arg_1): B(s_n_p_arg_0),D(s_n_p_arg_1) {}}
#define STRUCT_NAMED_TRIPLE(A,B,C,D,E,F,G) struct G {A B; C D; E F; inline G(): B(),D(),F() {} inline G(A s_n_p_arg_0, C s_n_p_arg_1, E s_n_p_arg_2): B(s_n_p_arg_0),D(s_n_p_arg_1),F(s_n_p_arg_2) {}}
#define STRUCT_NAMED_QUAD(A,B,C,D,E,F,G,H,I) struct I {A B; C D; E F; G H; inline I(): B(),D(),F(),H() {} inline I(A s_n_p_arg_0, C s_n_p_arg_1, E s_n_p_arg_2, G s_n_p_arg_3): B(s_n_p_arg_0),D(s_n_p_arg_1),F(s_n_p_arg_2),H(s_n_p_arg_3) {}}

//SHORTCUTS FOR std::map------------------------------------------------

template<typename A, typename B>
bool map_contains(const map<A,B>& m, const A& key)
{
  return m.find(key) != m.end();
}

template<typename B>
bool map_contains(const map<string,B>& m, const char* key)
{
  return m.find(string(key)) != m.end();
}

template<typename A, typename B>
B map_get(const map<A,B>& m, const A& key)
{
  typename map<A,B>::const_iterator it = m.find(key);
  if(it == m.end())
    Global::fatalError("map_get: key not found");
  return it->second;
}

template<typename B>
B map_get(const map<string,B>& m, const char* key)
{
  typename map<string,B>::const_iterator it = m.find(string(key));
  if(it == m.end())
    Global::fatalError(string("map_get: key \"") + string(key) + string("\" not found"));
  return it->second;
}

template<typename A, typename B>
B map_get(const map<A,B>& m, const A& key, const B& def)
{
  typename map<A,B>::const_iterator it = m.find(key);
  if(it == m.end())
    return def;
  return it->second;
}

template<typename B>
B map_get(const map<string,B>& m, const char* key, const B& def)
{
  typename map<string,B>::const_iterator it = m.find(string(key));
  if(it == m.end())
    return def;
  return it->second;
}


#endif
