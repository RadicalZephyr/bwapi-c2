# Possible Alternatives to C ABI

- Implement our own 32bit module mode that implements a separate
  client mode that allows batching and fixes other issues
- Deconstructing the C++ API layer on top of BWAPI and replacing it
  with a pure C API/ABI including module/client mode
