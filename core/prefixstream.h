/*
 * prefixstream.h
 * Author: davidwu
 */

//This is a simple streambuffer and an ostream that begins each written line with a prefix.
//Based on some code at http://codereview.stackexchange.com/questions/54845/filtering-streambuf

#ifndef PREFIXSTREAM_H_
#define PREFIXSTREAM_H_

#include <iostream>
#include <streambuf>
#include <iomanip>
#include <string>

class PrefixBuffer: public std::streambuf
{
  std::string prefix;
  std::string buffer;
  std::streambuf* sbuf;

  public:
  inline PrefixBuffer(std::string prefix, std::streambuf* s)
  :prefix(prefix),buffer(),sbuf(s)
  {}
  inline ~PrefixBuffer()
  {overflow('\n');}

  private:
  inline int_type overflow(int_type c)
  {
    if(traits_type::eq_int_type(traits_type::eof(),c))
      return traits_type::not_eof(c);
    buffer += c;
    switch(c)
    {
      case '\n':
      case '\r':
      {
        sbuf->sputn(prefix.c_str(), prefix.size());
        int_type rc = sbuf->sputn(buffer.c_str(), buffer.size());
        buffer.clear();
        return rc;
      }
      default:
        return c;
    }
  }

  inline int sync()
  {
    return sbuf->pubsync();
  }
};

class PrefixStream : public std::ostream
{
  PrefixBuffer buf;

  public:
  inline PrefixStream(std::string prefix, std::ostream &os)
  :std::ostream(),buf(prefix,os.rdbuf())
  {
    this->rdbuf(&buf);
  }
};

#endif /* PREFIXSTREAM_H_ */
