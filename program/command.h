
/*
 * command.h
 * Author: davidwu
 */

#ifndef COMMAND_H
#define COMMAND_H

#include "../core/global.h"

using namespace std;

namespace Command
{
  //Return a vector of the args that were part of the main command (not flags)
  //including the zeroth arg (the program name)
  vector<string> parseCommand(int argc, const char* const *argv, const string& help,
      const string& required, const string& allowed, const string& empty, const string& nonempty);

  //Same, but specify in space-separated string list
  //what flags are required, allowed, and what flags must have
  //empty or nonempty arguments. And if "-help" is specified, will output the help string.
  map<string,string> parseFlags(int argc, const char* const *argv, const string& help,
      const string& required, const string& allowed, const string& empty, const string& nonempty);
  map<string,string> parseFlags(int argc, const char* const *argv, const char* help,
      const char* required, const char* allowed, const char* empty, const char* nonempty);

  void printHelp(int argc, const char* const *argv, const string& help);

  bool isSet(const map<string,string>& flags, const char* flag);

  int getInt(const map<string,string>& flags, const char* flag);
  uint64_t getUInt64(const map<string,string>& flags, const char* flag);
  double getDouble(const map<string,string>& flags, const char* flag);
  string getString(const map<string,string>& flags, const char* flag);

  int getInt(const map<string,string>& flags, const char* flag, int def);
  uint64_t getUInt64(const map<string,string>& flags, const char* flag, uint64_t def);
  double getDouble(const map<string,string>& flags, const char* flag, double def);
  string getString(const map<string,string>& flags, const char* flag, const string& def);


  //Get the git revision id, if possible.
  string gitRevisionId();
}




#endif
