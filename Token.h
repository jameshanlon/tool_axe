// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#ifndef _Token_h_
#define _Token_h_

#include <stdint.h>

enum ControlTokenValue {
  CT_END    = 0x01,
  CT_PAUSE  = 0x02,
  CT_ACK    = 0x03,
  CT_NACK   = 0x04,
  CT_WRITEC = 0xc0,
  CT_READC  = 0xc1,
  CT_READN  = 0x10,
  CT_READ1  = 0x11,
  CT_READ2  = 0x12,
  CT_READ4  = 0x13,
  CT_READ8  = 0x14,
  CT_WRITEN = 0x15,
  CT_WRITE1 = 0x16,
  CT_WRITE2 = 0x17,
  CT_WRITE4 = 0x18,
  CT_WRITE8 = 0x19
};

class Token {
private:
  uint8_t value;
  bool control;
  
public:
  Token(uint8_t v = 0, bool c = false)
  : value(v), control(c) { }
  
  bool isControl() const
  {
    return control;
  }
  
  uint8_t getValue() const
  {
    return value;
  }
  
  operator uint8_t() const { return value; }
};

#endif // _Token_h_
