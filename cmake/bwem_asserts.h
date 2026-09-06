// Force-included (-include) into every translation unit when BWAPI_C2_BWEM_ASSERTS is ON, so
// BWEM's assertions are real assert() calls instead of nothing (plan section 8.3, R11.9).
//
// Upstream's defs.h has a BWEM_ASSERTS branch, but it defines only bwem_assert_plus and
// bwem_assert_debug_only and forgets bwem_assert and the two bwem_assert_throw forms, so
// defining the switch alone does not compile. This header defines the switch and supplies the
// three missing macros, with the same text as upstream's own else-branch; defs.h's
// BWEM_ASSERTS branch never redefines them, so there is no conflict. detail::onAssertThrowFailed
// is declared in defs.h before any use site, which is all a macro needs.
//
// Pair this with a Debug build: assert() is itself a no-op under NDEBUG.
#pragma once
#define BWEM_ASSERTS 1
#define bwem_assert(expr) bwem_assert_plus(expr, "")
#define bwem_assert_throw_plus(expr, message) \
  ((expr) ? (void)0 : ::BWEM::detail::onAssertThrowFailed(__FILE__, __LINE__, #expr, message))
#define bwem_assert_throw(expr) bwem_assert_throw_plus(expr, "")
