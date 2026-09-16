%module bwapi_tm2
%{
#include <BWAPI/Unit.h>
%}
// §4: handles are int32_t IDs, validated
%typemap(ctype) BWAPI::Unit, BWAPI::UnitInterface* "int"
%typemap(out)   BWAPI::Unit, BWAPI::UnitInterface* "$result = $1 ? $1->getID() : -1;"
%typemap(in)    BWAPI::Unit, BWAPI::UnitInterface* "$1 = BWAPI::Broodwar->getUnit($input);"

// §4: collections -> caller buffer + true-count return.  Try it.
%typemap(ctype) BWAPI::Unitset "int"
%typemap(out)   BWAPI::Unitset "$result = (int)$1.size(); /* where do out_ids and cap come from? */"

%include <BWAPI/Unit.h>
