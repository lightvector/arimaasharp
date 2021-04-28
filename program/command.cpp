
/*
 * command.cpp
 * Author: davidwu
 */

#include <sstream>
#include <cstdlib>
#include "../core/global.h"
#include "../program/command.h"
#include "../program/gitrevision.h"

using namespace std;

void Command::printHelp(int argc, const char* const *argv, const string& help)
{
  if(argc > 0)
    cout << argv[0] << " " << help << endl;
  else
    cout << help << endl;
}

vector<string> Command::parseCommand(int argc, const char* const *argv, const string& help,
    const string& required, const string& allowed, const string& empty, const string& nonempty)
{
  (void)required;
  (void)allowed;
  (void)nonempty;
  vector<string> empties = Global::split(empty);
  vector<string> vec;
  bool lastWasFlagWithArg = false;
  for(int i = 0; i<argc; i++)
  {
    const char* arg = argv[i];
    if(arg[0] == '\0')
    {
      printHelp(argc,argv,help);
      Global::fatalError("empty argument!");
    }
    if(arg[0] == '-')
    {
      lastWasFlagWithArg = true;
      for(size_t j = 0; j<empties.size(); j++)
        if(empties[j] == arg+1)
        {lastWasFlagWithArg = false; break;}
      continue;
    }
    if(lastWasFlagWithArg)
    {lastWasFlagWithArg = false; continue;}

    vec.push_back(string(arg));
  }

  return vec;
}

static map<string,string> parseFlagsHelper(int argc, const char* const *argv, const string& help,
    const string& required, const string& allowed, const string& empty, const string& nonempty)
{
  (void)required;
  (void)allowed;
  (void)nonempty;
  map<string,string> flagmap;

  bool onflag = false;
  string flagname;
  string flagval;
  vector<string> empties = Global::split(empty);

  for(int i = 1; i<argc; i++)
  {
    const char* arg = argv[i];
    if(arg[0] == '\0')
    {Command::printHelp(argc,argv,help); Global::fatalError("Empty argument!");}
    if(arg[0] == '-' && arg[1] == '\0')
    {Command::printHelp(argc,argv,help); Global::fatalError("Empty flag!");}

    //New flag!
    if(arg[0] == '-' && (arg[1] < '0' || arg[1] > '9'))
    {
      //Store old flag, if any
      if(onflag)
      {
        if(flagmap.find(flagname) != flagmap.end())
        {Command::printHelp(argc,argv,help); Global::fatalError("Duplicate argument!");}
        flagmap[flagname] = flagval;
      }

      //Grab flag name
      string s = string(arg);
      flagname = s.substr(1,s.length()-1);
      if(s.find_first_of(' ') != string::npos)
      {Command::printHelp(argc,argv,help); Global::fatalError("Flag has space character in it");}

      onflag = true;
      for(size_t j = 0; j<empties.size(); j++)
      {
        if(empties[j] == arg+1)
        {
          //Flag accepts no arguments, include it immediately
          onflag = false;
          flagmap[flagname] = string();
          break;
        }
      }

      flagval = string();
      continue;
    }

    //Value part of the current flag
    if(onflag)
    {
      flagval.append(arg);

      if(flagmap.find(flagname) != flagmap.end())
      {Command::printHelp(argc,argv,help); Global::fatalError("Duplicate argument!");}
      flagmap[flagname] = flagval;

      onflag = false;
    }
  }

  //Store old flag, if any
  if(onflag)
  {
    if(flagmap.find(flagname) != flagmap.end())
    {Command::printHelp(argc,argv,help); Global::fatalError("Duplicate argument!");}
    flagmap[flagname] = flagval;
  }

  if(flagmap.find("version") != flagmap.end())
  {
    cout << Command::gitRevisionId() << endl;
    cout << "Compiled " << __DATE__ << " " << __TIME__ << endl;
    exit(0);
  }

  return flagmap;
}

map<string,string> Command::parseFlags(int argc, const char* const *argv, const string& help,
    const string& required, const string& allowed, const string& empty, const string& nonempty)
{
  const map<string,string> flags = parseFlagsHelper(argc, argv, help, required, allowed, empty, nonempty);

  if(flags.find("help") != flags.end())
  {
    if(argc > 0)
      cout << argv[0] << " " << help << endl;
    else
      cout << help << endl;
    exit(0);
  }

  //Check that nonempty flags have actual data
  map<string,string> flagscopy = flags;
  string flag;

  istringstream inemp(empty);
  while(inemp.good())
  {
    flag = string();
    inemp >> flag;
    flag = Global::trim(flag);
    if(flag.length() == 0)
      break;
    map<string,string>::iterator it = flagscopy.find(flag);
    if(it != flagscopy.end())
    {
      if(it->second.length() != 0)
      {printHelp(argc,argv,help); Global::fatalError(string("Flag -") + flag + string(" takes no arguments"));}
    }
  }

  istringstream innon(nonempty);
  while(innon.good())
  {
    flag = string();
    innon >> flag;
    flag = Global::trim(flag);
    if(flag.length() == 0)
      break;
    map<string,string>::iterator it = flagscopy.find(flag);
    if(it != flagscopy.end())
    {
      if(it->second.length() == 0)
      {printHelp(argc,argv,help); Global::fatalError(string("Flag -") + flag + string(" specified without arguments"));}
    }
  }

  //Check that required strings are present
  istringstream inreq(required);
  while(inreq.good())
  {
    flag = string();
    inreq >> flag;
    flag = Global::trim(flag);
    if(flag.length() == 0)
      break;
    int numerased = flagscopy.erase(flag);
    if(numerased != 1)
    {printHelp(argc,argv,help); Global::fatalError(string("Required flag -") + flag + string(" not specified"));}
  }

  //Remove allowed flags
  istringstream inall(allowed);
  while(inall.good())
  {
    flag = string();
    inall >> flag;
    flag = Global::trim(flag);
    if(flag.length() == 0)
      break;
    flagscopy.erase(flag);
  }

  //Make sure no flags left over
  if(!flagscopy.empty())
  {printHelp(argc,argv,help); Global::fatalError(string("Unknown flag -") + flagscopy.begin()->first);}

  return flags;
}


map<string,string> Command::parseFlags(int argc, const char* const *argv, const char* help,
    const char* required, const char* allowed, const char* empty, const char* nonempty)
{
  return parseFlags(argc, argv, string(help), string(required), string(allowed),
      string(empty), string(nonempty));
}

bool Command::isSet(const map<string,string>& flags, const char* flag)
{
  return map_contains(flags,flag);
}

int Command::getInt(const map<string,string>& flags, const char* flag)
{
  int x;
  if(map_contains(flags,flag) && Global::tryStringToInt(flags.find(flag)->second,x))
    return x;
  Global::fatalError(string("Could not find flag ") + flag);
  return 0;
}
uint64_t Command::getUInt64(const map<string,string>& flags, const char* flag)
{
  uint64_t x;
  if(map_contains(flags,flag) && Global::tryStringToUInt64(flags.find(flag)->second,x))
    return x;
  Global::fatalError(string("Could not find flag ") + flag);
  return 0;
}
double Command::getDouble(const map<string,string>& flags, const char* flag)
{
  double x;
  if(map_contains(flags,flag) && Global::tryStringToDouble(flags.find(flag)->second,x))
    return x;
  Global::fatalError(string("Could not find flag ") + flag);
  return 0;
}
string Command::getString(const map<string,string>& flags, const char* flag)
{
  if(map_contains(flags,flag))
    return flags.find(flag)->second;
  Global::fatalError(string("Could not find flag ") + flag);
  return string();
}

int Command::getInt(const map<string,string>& flags, const char* flag, int def)
{
  if(map_contains(flags,flag))
  {
    int x;
    if(!Global::tryStringToInt(flags.find(flag)->second,x))
      Global::fatalError(string("Could not parse int for flag ") + flag + ":" + flags.find(flag)->second);
    return x;
  }
  return def;
}
uint64_t Command::getUInt64(const map<string,string>& flags, const char* flag, uint64_t def)
{
  if(map_contains(flags,flag))
  {
    uint64_t x;
    if(!Global::tryStringToUInt64(flags.find(flag)->second,x))
      Global::fatalError(string("Could not parse uint64_t for flag ") + flag + ":" + flags.find(flag)->second);
    return x;
  }
  return def;
}
double Command::getDouble(const map<string,string>& flags, const char* flag, double def)
{
  if(map_contains(flags,flag))
  {
    double x;
    if(!Global::tryStringToDouble(flags.find(flag)->second,x))
      Global::fatalError(string("Could not parse double for flag ") + flag + ":" + flags.find(flag)->second);
    return x;
  }
  return def;
}
string Command::getString(const map<string,string>& flags, const char* flag, const string& def)
{
  if(map_contains(flags,flag))
    return flags.find(flag)->second;
  return def;
}



string Command::gitRevisionId()
{
  #ifdef GIT_REVISION_ID
    return string(GIT_REVISION_ID);
  #else
    return string();
  #endif
}

