/* bwapi_c2_bwem.h -- the BWEM half of bwapi-c2: map analysis over the connected game.
 *
 * Same conventions as bwapi_c2.h, stated once at the top of bwapi_c2_types.h. Everything BWEM
 * exposes about areas, chokepoints, bases, neutrals and tiles is emitted into this header from
 * phase 1 on, after the R11.3 sketch; phase 0 declares the lifecycle (plan section 8.2).
 *
 * Handle spaces: bwapi_bwem_area_id is BWEM's own Area::Id (1..N; 0 means not walkable),
 * bwapi_bwem_choke_id its ChokePoint::Index() (0..N-1), bwapi_bwem_base_id an index the ABI
 * synthesises (0..N-1) because BWEM's Base has no identity of its own. All three are disjoint
 * from each other and from every id in bwapi_c2.h, with one deliberate exception: a BWEM
 * neutral IS a BWAPI unit and is addressed by its unit id, so the two headers join with no
 * lookup table on either side.
 *
 * Ids are stable from one bwapi_bwem_initialize() to the next. Nothing carries across a match.
 * BWEM has nothing per frame: its whole footprint is one call at match start and a teardown,
 * which bwapi_client_disconnect() performs if the host forgot.
 */
#ifndef BWAPI_C2_BWEM_H
#define BWAPI_C2_BWEM_H

#include "bwapi_c2_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- lifecycle -------------------------------------------------------------------------- */

/* Runs BWEM's whole analysis over the connected game: Initialize, then optionally
 * EnableAutomaticPathAnalysis and FindBasesForStartingLocations. One call replaces three so
 * the ordering cannot be got wrong. Resets first if already initialised, so the call produces
 * one freshly analysed map from any state. Blocks for roughly 450 ms on a 128x128 map.
 * Returns 1 on success. Before handing the game to BWEM it checks every neutral's tile
 * footprint against every other's; two that partially overlap (a map-editor accident BWEM
 * accepts silently and crashes on at teardown) latch BWAPI_ERR_BWEM with a message naming the
 * two units, and it returns 0 without initialising. Not connected: BWAPI_ERR_NOT_CONNECTED. */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_bwem_initialize(int32_t enable_path_analysis,
                                                         int32_t find_bases_for_start_locations) BWAPI_C2_NOEXCEPT;
/* 1 between a successful initialize and the next reset. */
BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_bwem_initialized(void) BWAPI_C2_NOEXCEPT;
/* Destroys the map and returns its memory (about 30 MB); initialized() is 0 afterwards.
 * Optional: initialize() and bwapi_client_disconnect() both do it. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_bwem_reset(void) BWAPI_C2_NOEXCEPT;

/* The three destruction hooks. The ABI's event pump already dispatches UnitDestroy to BWEM
 * with the right filter, so a host need not call these; they stay exported for hosts that
 * want control, and are safe with any unit id: a unit BWEM does not track is ignored, never an
 * assertion. Idempotent. */
BWAPI_C2_API void BWAPI_C2_CALL bwapi_bwem_on_mineral_destroyed(bwapi_unit_id unit_id) BWAPI_C2_NOEXCEPT;
BWAPI_C2_API void BWAPI_C2_CALL bwapi_bwem_on_static_building_destroyed(bwapi_unit_id unit_id) BWAPI_C2_NOEXCEPT;
BWAPI_C2_API void BWAPI_C2_CALL bwapi_bwem_on_blocking_neutral_destroyed(bwapi_unit_id unit_id) BWAPI_C2_NOEXCEPT;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* BWAPI_C2_BWEM_H */
