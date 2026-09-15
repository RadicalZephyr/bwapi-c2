%module bwapi_tm
%{
#include <BWAPI/Unit.h>
%}

// §4: positions return packed in one int64_t
%typemap(ctype)  BWAPI::Position, Position "long long"
%typemap(out)    BWAPI::Position, Position
  "$result = (long long)(((unsigned long long)(unsigned)$1.y << 32) | (unsigned)$1.x);"

// §4: booleans are int32_t, never C++ bool
%typemap(ctype)  bool "int"
%typemap(out)    bool "$result = $1 ? 1 : 0;"

%ignore BWAPI::UnitInterface::getClientInfo;
%include <BWAPI/Unit.h>
