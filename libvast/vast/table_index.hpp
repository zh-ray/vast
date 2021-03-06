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

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <caf/expected.hpp>
#include <caf/fwd.hpp>

#include "vast/column_index.hpp"
#include "vast/filesystem.hpp"
#include "vast/ids.hpp"
#include "vast/type.hpp"

#include "vast/detail/range.hpp"

namespace vast {

// -- free functions -----------------------------------------------------------

/// Creates a column layout for the given type.
/// @relates table_index
caf::expected<table_index> make_table_index(caf::actor_system& sys,
                                            path filename, record_type layout);

// -- class definition ---------------------------------------------------------

/// Wraps multiple [@ref column_index](column indexes) according to a layout.
class table_index {
public:
  // -- friend declarations ----------------------------------------------------

  friend caf::expected<table_index> make_table_index(caf::actor_system&, path,
                                                     record_type);

  // -- member and nested types ------------------------------------------------

  /// Stores column indexes.
  using columns_vector = std::vector<column_index_ptr>;

  /// Identifies a subset of columns.
  using columns_range = detail::iterator_range<columns_vector::iterator>;

  // -- constructors, destructors, and assignment operators --------------------

  table_index(caf::actor_system& sys);

  table_index(table_index&&) = default;

  ~table_index() noexcept;

  // -- persistence ------------------------------------------------------------

  /// Load state from disk.
  caf::error init();

  /// Persists all indexes to disk.
  caf::error flush_to_disk();

  /// -- properties ------------------------------------------------------------

  /// @returns the number of columns.
  size_t num_columns() const {
    return columns_.size();
  }

  /// @returns the column at given index.
  /// @pre `column_size < num_columns()`
  column_index& at(size_t column_index);

  /// @returns the column at given index and creates it lazily from the factory
  ///          in case it doesn't yet exist.
  /// @pre `column_size < num_columns()`
  template <class Factory, class Continuation>
  auto with_column(size_t column_index, Factory& factory, Continuation f) {
    VAST_ASSERT(column_index < columns_.size());
    auto& col = columns_[column_index];
    using result_type = decltype(f(*col));
    if (col == nullptr) {
      auto fac_res = factory();
      if (!fac_res)
        return result_type{std::move(fac_res.error())};
      col = std::move(*fac_res);
    }
    return f(*col);
  }

  /// Applies `f` to each column pointer.
  template <class F>
  void for_each_column(F f) {
    for (auto& col : columns_)
      f(col.get());
  }

  /// @returns a pointer the column with given name or `nullptr` if no such
  ///          column exists.
  column_index* by_name(std::string_view column_name);

  /// @returns the base directory for all stored column indexes.
  const path& base_dir() const {
    return base_dir_;
  }

  /// @returns the type defining this table's layout.
  const record_type& layout() const {
    // Always safe, because the only way to construct a table_index is with a
    // record_type.
    return caf::get<record_type>(type_erased_layout_);
  }

  /// @returns whether `add` was called at least once.
  bool dirty() const {
    return dirty_;
  }

  /// @returns the base directory for meta column indexes.
  path meta_dir() const;

  /// @returns the base directory for data column indexes.
  path data_dir() const;

  /// Indexes a slice for all columns.
  /// @param x Table slice for ingestion.
  caf::error add(const table_slice_ptr& x);

  /// Queries event IDs that fulfill the given predicate on any column.
  /// @pre `init()` was called previously.
  caf::expected<bitmap> lookup(const predicate& pred);

  /// Queries event IDs that fulfill the given expression.
  /// @pre `init()` was called previously.
  caf::expected<bitmap> lookup(const expression& expr);

private:
  // -- dispatch functions -----------------------------------------------------

  caf::expected<bitmap> lookup_impl(const predicate& pred);

  caf::expected<bitmap> lookup_impl(const expression& expr);

  caf::expected<bitmap> lookup_impl(const predicate& pred,
                                    const attribute_extractor& ex,
                                    const data& x);

  caf::expected<bitmap> lookup_impl(const predicate& pred,
                                    const data_extractor& dx, const data& x);

  // -- constructors, destructors, and assignment operators --------------------

  table_index(caf::actor_system& sys, record_type layout, path base_dir);

  // -- member variables -------------------------------------------------------

  /// Stores `layout_` in a type-erased handle. We need this type-erased
  /// representation in a few instances such as expression visitors.
  type type_erased_layout_;

  /// Columns of our type-dependant layout. Lazily filled for columns data to
  /// delay file I/O until a column is accessed by the user.
  columns_vector columns_;

  /// Base directory for all children column indexes.
  path base_dir_;

  /// Allows a shortcut in `add` if all columns are initialized.
  bool dirty_;

  /// Stores what IDs are present in this table.
  ids row_ids_;

  /// Hosting actor system.
  caf::actor_system& sys_;
};

// -- free functions -----------------------------------------------------------

} // namespace vast
