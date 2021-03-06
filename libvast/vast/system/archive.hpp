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

#include <vector>

#include <caf/fwd.hpp>
#include <caf/replies_to.hpp>
#include <caf/stateful_actor.hpp>
#include <caf/typed_actor.hpp>
#include <caf/typed_event_based_actor.hpp>

#include "vast/fwd.hpp"
#include "vast/ids.hpp"
#include "vast/store.hpp"
#include "vast/system/atoms.hpp"

namespace vast::system {

/// @relates archive
struct archive_state {
  std::unique_ptr<vast::store> store;
  static inline const char* name = "archive";
};

/// @relates archive
using archive_type = caf::typed_actor<
  caf::reacts_to<caf::stream<table_slice_ptr>>,
  caf::replies_to<ids>::with<std::vector<event>>,
  caf::replies_to<status_atom>::with<caf::dictionary<caf::config_value>>
>;

/// Stores event batches and answers queries for ID sets.
/// @param self The actor handle.
/// @param dir The root directory of the archive.
/// @param capacity The number of segments to cache in memory.
/// @param max_segment_size The maximum segment size in bytes.
/// @pre `max_segment_size > 0`
archive_type::behavior_type
archive(archive_type::stateful_pointer<archive_state> self, path dir,
        size_t capacity, size_t max_segment_size);

} // namespace vast::system

