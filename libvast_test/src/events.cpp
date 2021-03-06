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

#include "fixtures/events.hpp"

#include "vast/default_table_slice.hpp"
#include "vast/format/bgpdump.hpp"
#include "vast/format/bro.hpp"
#include "vast/format/test.hpp"
#include "vast/table_slice_builder.hpp"
#include "vast/to_events.hpp"
#include "vast/type.hpp"

#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/data.hpp"
#include "vast/concept/printable/vast/event.hpp"

namespace fixtures {

namespace {

timestamp epoch;

std::vector<event> make_ascending_integers(size_t count) {
  std::vector<event> result;
  type layout{record_type{{"value", integer_type{}}}};
  layout.name("test::int");
  for (size_t i = 0; i < count; ++i) {
    result.emplace_back(event::make(vector{static_cast<integer>(i)}, layout));
    result.back().timestamp(epoch + std::chrono::seconds(i));
  }
  return result;
}

std::vector<event> make_alternating_integers(size_t count) {
  std::vector<event> result;
  type layout{record_type{{"value", integer_type{}}}};
  layout.name("test::int");
  for (size_t i = 0; i < count; ++i) {
    result.emplace_back(event::make(vector{static_cast<integer>(i % 2)},
                                    layout));
    result.back().timestamp(epoch + std::chrono::seconds(i));
  }
  return result;
}

/// insert item into a sorted vector
/// @precondition is_sorted(vec)
/// @postcondition is_sorted(vec)
template <typename T, typename Pred>
auto insert_sorted(std::vector<T>& vec, const T& item, Pred pred) {
  return vec.insert(std::upper_bound(vec.begin(), vec.end(), item, pred), item);
}

} // namespace <anonymous>

size_t events::slice_size = 8;

std::vector<event> events::bro_conn_log;
std::vector<event> events::bro_dns_log;
std::vector<event> events::bro_http_log;
std::vector<event> events::bgpdump_txt;
std::vector<event> events::random;

std::vector<table_slice_ptr> events::bro_conn_log_slices;
std::vector<table_slice_ptr> events::bro_dns_log_slices;
std::vector<table_slice_ptr> events::bro_http_log_slices;
std::vector<table_slice_ptr> events::bgpdump_txt_slices;
// std::vector<table_slice_ptr> events::random_slices;

std::vector<event> events::ascending_integers;
std::vector<table_slice_ptr> events::ascending_integers_slices;

std::vector<event> events::alternating_integers;
std::vector<table_slice_ptr> events::alternating_integers_slices;

record_type events::bro_conn_log_layout() {
  return bro_conn_log_slices[0]->layout();
}

std::vector<table_slice_ptr>
events::copy(std::vector<table_slice_ptr> xs) {
  std::vector<table_slice_ptr> result;
  result.reserve(xs.size());
  for (auto& x : xs)
    result.emplace_back(x->copy());
  return result;
}

/// A wrapper around a table_slice_builder_ptr that assigns ids to each
/// added event.
class id_assigning_builder {
public:
  explicit id_assigning_builder(table_slice_builder_ptr b) : inner_{b} {
    // nop
  }

  /// Adds an event to the table slice and assigns an id.
  bool add(event& e) {
    if (!inner_->add(e.timestamp()))
      FAIL("builder->add() failed");
    if (!inner_->recursive_add(e.data(), e.type()))
      FAIL("builder->recursive_add() failed");
    e.id(id_++);
    return true;
  }

  auto rows() const {
    return inner_->rows();
  }

  bool start_slice(size_t offset) {
    if (rows() != 0)
      return false;
    offset_ = offset;
    id_ = offset;
    return true;
  }

  /// Finish the slice and set its offset.
  table_slice_ptr finish() {
    auto slice = inner_->finish();
    slice.unshared().offset(offset_);
    return slice;
  }

private:
  table_slice_builder_ptr inner_;
  size_t offset_ = 0;
  size_t id_ = 0;
};

/// Tries to access the builder for `layout`.
class builders {
public:
  using map_type = std::map<std::string, id_assigning_builder>;

  id_assigning_builder* get(const type& layout) {
    auto i = builders_.find(layout.name());
    if (i != builders_.end())
      return &i->second;
    return caf::visit(
      detail::overload(
        [&](const record_type& rt) -> id_assigning_builder* {
          // We always add a timestamp as first column to the layout.
          auto internal = rt;
          record_field tstamp_field{"timestamp", timestamp_type{}};
          internal.fields.insert(internal.fields.begin(),
                                 std::move(tstamp_field));
          id_assigning_builder tmp{
            default_table_slice::make_builder(std::move(internal))};
          return &(
            builders_.emplace(layout.name(), std::move(tmp)).first->second);
        },
        [&](const auto&) -> id_assigning_builder* {
          FAIL("layout is not a record type");
          return nullptr;
        }),
      layout);
  }

  map_type& all() {
    return builders_;
  }

private:
  map_type builders_;
};

events::events() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  MESSAGE("inhaling unit test suite events");
  bro_conn_log = inhale<format::bro::reader>(bro::small_conn);
  REQUIRE_EQUAL(bro_conn_log.size(), 20u);
  bro_dns_log = inhale<format::bro::reader>(bro::dns);
  REQUIRE_EQUAL(bro_dns_log.size(), 32u);
  bro_http_log = inhale<format::bro::reader>(bro::http);
  REQUIRE_EQUAL(bro_http_log.size(), 40u);
  bgpdump_txt = inhale<format::bgpdump::reader>(bgpdump::updates20180124);
  REQUIRE_EQUAL(bgpdump_txt.size(), 100u);
  random = extract(vast::format::test::reader{42, 1000});
  REQUIRE_EQUAL(random.size(), 1000u);
  ascending_integers = make_ascending_integers(250);
  alternating_integers = make_alternating_integers(250);
  auto allocate_id_block = [i = id{0}](size_t size) mutable {
    auto first = i;
    i += size;
    return first;
  };
  MESSAGE("building slices of " << slice_size << " events each");
  auto assign_ids_and_slice_up = [&](std::vector<event>& src) {
    VAST_ASSERT(src.size() > 0);
    VAST_ASSERT(caf::holds_alternative<record_type>(src[0].type()));
    std::vector<table_slice_ptr> slices;
    builders bs;
    auto finish_slice = [&](auto& builder) {
      insert_sorted(slices, builder.finish(),
                    [](const auto& lhs, const auto& rhs) {
                      return lhs->offset() < rhs->offset();
                    });
    };
    for (auto& e : src) {
      auto bptr = bs.get(e.type());
      if (bptr->rows() == 0)
        bptr->start_slice(allocate_id_block(slice_size));
      bptr->add(e);
      if (bptr->rows() == slice_size)
        finish_slice(*bptr);
    }
    for (auto& i : bs.all()) {
      auto builder = i.second;
      if (builder.rows() > 0)
        finish_slice(builder);
    }
    return slices;
  };
  bro_conn_log_slices = assign_ids_and_slice_up(bro_conn_log);
  bro_dns_log_slices = assign_ids_and_slice_up(bro_dns_log);
  allocate_id_block(1000); // cause an artificial gap in the ID sequence
  bro_http_log_slices = assign_ids_and_slice_up(bro_http_log);
  bgpdump_txt_slices = assign_ids_and_slice_up(bgpdump_txt);
  //random_slices = slice_up(random);
  ascending_integers_slices = assign_ids_and_slice_up(ascending_integers);
  alternating_integers_slices = assign_ids_and_slice_up(alternating_integers);
  auto sort_by_id = [](std::vector<event>& v) {
    std::sort(
      v.begin(), v.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.id() < rhs.id(); });
  };
  auto as_events = [&](const auto& slices) {
    std::vector<event> result;
    for (auto& slice : slices) {
      auto xs = to_events(*slice);
      std::move(xs.begin(), xs.end(), std::back_inserter(result));
    }
    return result;
  };
#define SANITY_CHECK(event_vec, slice_vec)                                     \
  {                                                                            \
    auto flat_log = as_events(slice_vec);                                      \
    auto sorted_event_vec = event_vec;                                         \
    sort_by_id(sorted_event_vec);                                              \
    REQUIRE_EQUAL(sorted_event_vec.size(), flat_log.size());                   \
    for (size_t i = 0; i < sorted_event_vec.size(); ++i) {                     \
      if (flatten(sorted_event_vec[i]) != flat_log[i]) {                       \
        FAIL(#event_vec << " != " << #slice_vec << "\ni: " << i << '\n'        \
                        << to_string(sorted_event_vec[i])                      \
                        << " != " << to_string(flat_log[i]));                  \
      }                                                                        \
    }                                                                          \
  }
  SANITY_CHECK(bro_conn_log, bro_conn_log_slices);
  SANITY_CHECK(bro_dns_log, bro_dns_log_slices);
  SANITY_CHECK(bro_http_log, bro_http_log_slices);
  SANITY_CHECK(bgpdump_txt, bgpdump_txt_slices);
  //SANITY_CHECK(random, const_random_slices);
}

} // namespace fixtures
