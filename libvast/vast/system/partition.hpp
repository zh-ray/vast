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

#include <functional>

#include <caf/detail/unordered_flat_map.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/fwd.hpp>

#include "vast/aliases.hpp"
#include "vast/filesystem.hpp"
#include "vast/fwd.hpp"
#include "vast/system/indexer_manager.hpp"
#include "vast/type.hpp"
#include "vast/uuid.hpp"

namespace vast::system {

/// The horizontal data scaling unit of the index. A partition represents a
/// slice of indexes for a specific ID interval.
class partition : public caf::ref_counted {
public:
  // -- friends ----------------------------------------------------------------

  friend class indexer_manager;

  // -- member types -----------------------------------------------------------

  /// Persistent meta state for the partition.
  struct meta_data {
    /// Maps type digests (used as directory names) to layouts (i.e. record
    /// types).
    caf::detail::unordered_flat_map<std::string, record_type> types;

    /// Stores whether we modified `types` after loading it.
    bool dirty = false;
  };

  // -- constructors, destructors, and assignment operators --------------------

  /// @param base_dir The base directory for all partition. This partition will
  ///                 save to and load from `base_dir / id`.
  /// @param id Unique identifier for this partition.
  /// @param factory Factory function for INDEXER actors.
  partition(caf::actor_system& sys, const path& base_dir, uuid id,
            indexer_manager::indexer_factory factory);

  ~partition() noexcept override;

  // -- persistence ------------------------------------------------------------

  caf::error flush_to_disk();

  // -- properties -------------------------------------------------------------

  /// @returns the INDEXER manager.
  indexer_manager& manager() noexcept {
    return mgr_;
  }

  /// @returns the INDEXER manager.
  const indexer_manager& manager() const noexcept {
    return mgr_;
  }

  /// @returns whether the meta data was changed.
  bool dirty() const noexcept {
    return meta_data_.dirty;
  }

  /// @returns the working directory of the partition.
  const path& dir() const noexcept {
    return dir_;
  }
  /// @returns the unique ID of the partition.
  const uuid& id() const noexcept {
    return id_;
  }

  /// @returns all layouts in this partition.
  std::vector<record_type> layouts() const;

  /// @returns the file name for saving or loading the ::meta_data.
  path meta_file() const;

  // -- operations -------------------------------------------------------------

  /// Checks what layout could match `expr` and calls
  /// `self->request(...).then(f)` for each matching INDEXER.
  /// @returns the number of matched INDEXER actors.
  template <class F>
  size_t lookup_requests(caf::event_based_actor* self, const expression& expr,
                         F callback) {
    return mgr_.for_each_match(expr, [&](caf::actor& indexer) {
      self->request(indexer, caf::infinite, expr).then(callback);
    });
  }

  /// @returns all INDEXER actors that match the expression `expr`.
  size_t get_indexers(std::vector<caf::actor>& indexers,
                      const expression& expr);

  /// @returns all INDEXER actors that match the expression `expr`.
  std::vector<caf::actor> get_indexers(const expression& expr);

private:
  /// Called from the INDEXER manager whenever a new layout gets added during
  /// ingestion.
  void add_layout(const std::string& digest, const record_type& t) {
    if (meta_data_.types.emplace(digest, t).second && !meta_data_.dirty)
      meta_data_.dirty = true;
  }

  /// Spawns one INDEXER per type in the partition (lazily).
  indexer_manager mgr_;

  /// Keeps track of row types in this partition.
  meta_data meta_data_;

  /// Directory for persisting the meta data.
  path dir_;

  /// Uniquely identifies this partition.
  uuid id_;

  /// Hosting actor system.
  caf::actor_system& sys_;
};

// -- related types ------------------------------------------------------------

/// @relates partition
using partition_ptr = caf::intrusive_ptr<partition>;

// -- free functions -----------------------------------------------------------

/// @relates partition::meta_data
template <class Inspector>
auto inspect(Inspector& f, partition::meta_data& x) {
  return f(x.types);
}

/// Creates a partition.
/// @relates partition
partition_ptr make_partition(caf::actor_system& sys, const path& base_dir,
                             uuid id, indexer_manager::indexer_factory f);

/// Creates a partition that spawns regular INDEXER actors as children of
/// `self`.
/// @relates partition
partition_ptr make_partition(caf::actor_system& sys, caf::local_actor* self,
                             const path& base_dir, uuid id);

} // namespace vast::system

namespace std {

template <>
struct hash<vast::system::partition_ptr> {
  size_t operator()(const vast::system::partition_ptr& ptr) const;
};

} // namespace std
