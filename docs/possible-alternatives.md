# Possible Alternatives to C ABI

- Implement our own 32bit module mode that implements a separate
  client mode that allows batching and fixes other issues —
  worked through in [`proxy-protocol-design.md`](proxy-protocol-design.md),
  on the measurements in [R12](research/r12-proxy-transport.md)
- Deconstructing the C++ API layer on top of BWAPI and replacing it
  with a pure C API/ABI including module/client mode
