#!/usr/bin/env python3
"""R12: per-field FFI reads vs one bulk snapshot, from the host language.

ctypes is the pessimistic bound and the one every Python consumer gets for free;
cffi is measured too when installed. Run via run-transport-bench.sh, which builds
ffi_shape.c beside it and passes the .so path.
"""
import ctypes, sys, timeit

SO = sys.argv[1] if len(sys.argv) > 1 else "./ffi_shape.so"
UNITS, FIELDS, FRAME_S = 400, 20, 0.042      # a mid-game army; a "fastest" frame

lib = ctypes.CDLL(SO)
lib.get_field.restype = ctypes.c_int
lib.get_field.argtypes = [ctypes.c_int, ctypes.c_int]
lib.snapshot.argtypes = [ctypes.c_void_p, ctypes.c_int]

n = 200000
per_call = timeit.timeit(lambda: lib.get_field(7, 3), number=n) / n
per_frame = per_call * UNITS * FIELDS
print(f"  ctypes single FFI call            {per_call*1e9:8.1f} ns")
print(f"    x {UNITS} units x {FIELDS} fields  = {per_frame*1e3:8.2f} ms/frame"
      f"   ({per_frame/FRAME_S*100:5.1f}% of a 42 ms frame)")

buf = ctypes.create_string_buffer(UNITS * 336)
bulk = timeit.timeit(lambda: lib.snapshot(buf, UNITS), number=20000) / 20000
print(f"\n  one bulk snapshot of {UNITS} units    {bulk*1e6:8.2f} us"
      f"   ({bulk/FRAME_S*100:7.3f}% of a 42 ms frame)")
print(f"  bulk speedup over per-field       {per_frame/bulk:8.0f}x")

try:
    import cffi
except ImportError:
    print("\n  (cffi not installed: ctypes is the pessimistic bound; cffi is ~5-10x faster,")
    print("   which moves the per-frame cost but not the conclusion)")
else:
    ffi = cffi.FFI()
    ffi.cdef("int get_field(int,int); int snapshot(void*,int);")
    c = ffi.dlopen(SO)
    pc = timeit.timeit(lambda: c.get_field(7, 3), number=n) / n
    pf = pc * UNITS * FIELDS
    print(f"\n  cffi single FFI call              {pc*1e9:8.1f} ns")
    print(f"    x {UNITS} units x {FIELDS} fields  = {pf*1e3:8.2f} ms/frame"
          f"   ({pf/FRAME_S*100:5.1f}% of a 42 ms frame)")
