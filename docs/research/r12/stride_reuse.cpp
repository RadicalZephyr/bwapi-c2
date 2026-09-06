// R12: does the section-4 struct-array convention survive a reused buffer?
//
// write_rows() reads the stride from element zero's size and write_row() then writes the bytes
// filled into every row's size, element zero included. The two directions share one field. This
// probe reproduces abi_internal.h's two helpers verbatim, then drives them the way section 4
// and section 14 tell a consumer to drive them: size the buffer once, reuse it every frame.
//
// Three consumer/library pairings, since the size prefix exists for exactly the mismatched ones:
// a consumer whose row matches the library's, one compiled against a NEWER header than the
// library it loaded (host row larger), and one against an OLDER header (host row smaller).
// Built and run by run-stride-reuse.sh; takes no arguments.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ---- verbatim from src/abi_internal.h -----------------------------------------------------
template <class Row, class Fill>
void write_row(void* dst, int32_t stride, Fill fill) {
  const size_t filled = std::min(static_cast<size_t>(stride), sizeof(Row));
  Row row{};
  fill(row);
  row.size = static_cast<int32_t>(filled);
  char* bytes = static_cast<char*>(dst);
  std::memcpy(bytes, &row, filled);
  std::memset(bytes + filled, 0, static_cast<size_t>(stride) - filled);
}

template <class Row, class Fill>
int32_t write_rows(Row* out, int32_t cap, int32_t total, Fill fill) {
  if (cap <= 0) return total;
  const int32_t stride = out[0].size;
  char* dst = reinterpret_cast<char*>(out);
  const int32_t n = std::min(cap, total);
  for (int32_t i = 0; i < n; ++i, dst += stride)
    write_row<Row>(dst, stride, [&](Row& row) { fill(row, i); });
  return total;
}

template <class Row, class Fill>
void write_struct(Row* out, Fill fill) { write_row<Row>(out, out->size, fill); }
// -------------------------------------------------------------------------------------------

// The library's row, as the DLL was compiled: the v1 bwapi_event of section 5.6.
struct lib_row { int32_t size, type, unit_id, player_id, x, y, is_winner; };
// v2, one field appended under the append-only policy.
struct row_v2  { int32_t size, type, unit_id, player_id, x, y, is_winner, added_in_v2; };
// v0, one field short: a consumer built before is_winner existed.
struct row_v0  { int32_t size, type, unit_id, player_id, x, y; };

static const int32_t ROWS = 3, CAP = 4;

// One consumer: allocate a buffer of HostRow at its own stride, set element zero's size once as
// section 4 says, then call twice reusing it. Returns true when frame 2's rows are all correct.
template <class HostRow>
bool run_array(const char* label) {
  alignas(8) char buf[CAP * sizeof(HostRow)];
  std::memset(buf, 0, sizeof buf);
  HostRow* rows = reinterpret_cast<HostRow*>(buf);
  const int32_t host_stride = static_cast<int32_t>(sizeof(HostRow));

  rows[0].size = host_stride;                       // set once, per section 4
  write_rows(reinterpret_cast<lib_row*>(buf), CAP, ROWS,
             [](lib_row& r, int32_t i) { r.type = 100 + i; });
  const int32_t stride_after = rows[0].size;

  write_rows(reinterpret_cast<lib_row*>(buf), CAP, ROWS,   // frame 2, buffer reused
             [](lib_row& r, int32_t i) { r.type = 200 + i; });

  bool ok = true;
  for (int32_t i = 0; i < ROWS; ++i) if (rows[i].type != 200 + i) ok = false;
  printf("  %-34s host stride %2d, lib row %2zu, stride on frame 2 %2d -> %s\n",
         label, host_stride, sizeof(lib_row), stride_after, ok ? "rows correct" : "ROWS CORRUPT");
  if (!ok) for (int32_t i = 0; i < ROWS; ++i)
    printf("      row %d: size=%-3d type=%-4d expected %d%s\n", i, rows[i].size, rows[i].type,
           200 + i, rows[i].type == 200 + i ? "" : "   <-- wrong");
  return ok;
}

// The same reuse against the single-struct form, to place the boundary of the defect.
template <class HostRow>
bool run_struct(const char* label) {
  alignas(8) char buf[sizeof(HostRow)];
  std::memset(buf, 0, sizeof buf);
  HostRow* one = reinterpret_cast<HostRow*>(buf);
  one->size = static_cast<int32_t>(sizeof(HostRow));
  write_struct(reinterpret_cast<lib_row*>(buf), [](lib_row& r) { r.type = 100; });
  const int32_t size_after = one->size;
  write_struct(reinterpret_cast<lib_row*>(buf), [](lib_row& r) { r.type = 200; });
  const bool ok = one->type == 200;
  printf("  %-34s size on frame 2 %2d -> %s\n", label, size_after,
         ok ? "value correct" : "VALUE CORRUPT");
  return ok;
}

int main() {
  printf("array-out, one buffer reused across two frames:\n");
  bool matched = run_array<lib_row>("consumer matches library");
  bool newer   = run_array<row_v2>("consumer NEWER than library");
  bool older   = run_array<row_v0>("consumer OLDER than library");
  printf("single struct out, reused across two frames:\n");
  bool s_newer = run_struct<row_v2>("consumer NEWER than library");
  printf("\nresult: array reuse is %s for a matched consumer, %s for a newer one, %s for an\n"
         "older one; the single-struct form is %s for a newer one.\n",
         matched ? "safe" : "BROKEN", newer ? "safe" : "BROKEN", older ? "safe" : "BROKEN",
         s_newer ? "safe" : "BROKEN");
  return (matched && older && s_newer && !newer) ? 0 : 2;   // 0 confirms the expected shape
}
