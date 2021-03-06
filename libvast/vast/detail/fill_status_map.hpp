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

#include <caf/fwd.hpp>

namespace vast::detail {

/// Fills `xs` state from the stream manager `mgr`.
void fill_status_map(caf::dictionary<caf::config_value>& xs,
                     caf::stream_manager& mgr);

/// Fills `xs` state from `self`.
void fill_status_map(caf::dictionary<caf::config_value>& xs,
                     caf::scheduled_actor* self);

} // namespace vast::detail
