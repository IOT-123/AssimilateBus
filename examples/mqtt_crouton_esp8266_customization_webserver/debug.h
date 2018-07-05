#ifndef debug_h
#define debug_h
#include "Arduino.h"
#define DBG_OUTPUT_PORT Serial
class Debug
{
  public:
    Debug(byte flag){
      _flag = flag;
    }
void out_char(char *msg, bool linefeed, int flag){
  if (flag <= _flag){
    DBG_OUTPUT_PORT.print(msg);
    if (linefeed){
      DBG_OUTPUT_PORT.println();
    }
  }
}

void out_str(String msg, bool linefeed, int flag){
  if (flag <= _flag){
    DBG_OUTPUT_PORT.print(msg);
    if (linefeed){
      DBG_OUTPUT_PORT.println();
    }
  }
}

void out_fla(const __FlashStringHelper* msg, bool linefeed, int flag){
  if (flag <= _flag){
    DBG_OUTPUT_PORT.print(msg);
    if (linefeed){
      DBG_OUTPUT_PORT.println();
    }
  }
}
   
  private:
    byte _flag = 0;
    
};
#endif
