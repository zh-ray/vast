/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#pragma once

#include <unordered_map>
#include <vector>

#include <caf/actor.hpp>
#include <caf/stateful_actor.hpp>

#include "vast/expression.hpp"
#include "vast/meta_index.hpp"
#include "vast/fwd.hpp"
#include "vast/system/indexer_stage_driver.hpp"
#include "vast/system/partition.hpp"
#include "vast/uuid.hpp"

#include "vast/detail/flat_lru_cache.hpp"
#include "vast/detail/flat_set.hpp"

namespace vast::system {

/// State of an INDEX actor.
struct index_state {
  // -- member types -----------------------------------------------------------

  /// Pointer to the stage that multiplexing traffic between our sources and
  /// the INDEXER actors of the current partition.
  using stage_ptr = indexer_stage_driver::stage_ptr_type;

  /// Looks up partitions in the LRU cache by UUID.
  class partition_lookup {
  public:
    auto operator()(const uuid& id) const {
      return [&](const partition_ptr& ptr) {
        return ptr->id() == id;
      };
    }
  };

  /// Loads partitions from disk by UUID.
  class partition_factory {
  public:
    partition_factory(index_state* st) : st_(st) {
      // nop
    }

    partition_ptr operator()(const uuid& id) const;

  private:
    index_state* st_;
  };

  using partition_cache_type = detail::flat_lru_cache<partition_ptr,
                                                      partition_lookup,
                                                      partition_factory>;

  /// Stores context information for unfinished queries.
  struct lookup_state {
    /// Issued query.
    expression expr;

    /// Unscheduled partitions.
    std::vector<uuid> partitions;
  };

  // -- constructors, destructors, and assignment operators --------------------

  index_state();

  ~index_state();

  /// Initializes the state.
  caf::error init(caf::event_based_actor* self, const path& dir,
                  size_t max_events, size_t max_parts, size_t taste_parts);

  // -- persistence ------------------------------------------------------------

  /// Loads the state from disk.
  caf::error load_from_disk();

  /// Persists the state to disk.
  caf::error flush_to_disk();

  // -- convenience functions --------------------------------------------------

  /// Returns the file name for saving or loading the meta index.
  path meta_index_filename() const;

  /// @returns whether there's an idle worker available.
  bool worker_available();

  /// Takes the next worker from the idle workers stack and returns it.
  /// @pre `has_worker()`
  caf::actor next_worker();

  /// @returns various status metrics.
  caf::dictionary<caf::config_value> status() const;

  // -- member variables -------------------------------------------------------

  /// Allows to select partitions with timestamps.
  meta_index meta_idx;

  /// Our current partition.
  partition_ptr active;

  /// Recently accessed partitions.
  partition_cache_type lru_partitions;

  /// Base directory for all partitions of the index.
  path dir;

  /// Stream manager for ingesting events.
  stage_ptr stage;

  /// Pointer to the parent actor.
  caf::event_based_actor* self;

  /// The maximum number of events per partition.
  size_t max_partition_size;

  /// The number of partitions to schedule immediately for each query
  size_t taste_partitions;

  /// Allows the index to multiplex between waiting for ready workers and
  /// queries.
  caf::behavior has_worker;

  /// Maps query IDs to pending lookup state.
  std::unordered_map<uuid, lookup_state> pending;

  /// Stores partitions that are no longer active but have not persisted their
  /// state yet.
  std::vector<std::pair<partition_ptr, size_t>> unpersisted;

  /// Caches idle workers.
  std::vector<caf::actor> idle_workers;

  /// Name of the INDEX actor.
  static inline const char* name = "index";
};

/// Indexes events in horizontal partitions.
/// @param dir The directory of the index.
/// @param max_partition_size The maximum number of events per partition.
/// @param in_mem_partitions The maximum number of partitions to hold in memory.
/// @param taste_partitions The number of partitions to schedule immediately
///                         for each query
/// @pre `max_partition_size > 0 && in_mem_partitions > 0`
caf::behavior index(caf::stateful_actor<index_state>* self, const path& dir,
                    size_t max_partition_size, size_t in_mem_partitions,
                    size_t taste_partitions, size_t num_workers);

} // namespace vast::system

