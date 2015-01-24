#ifndef VAST_ACTOR_ACTOR_H
#define VAST_ACTOR_ACTOR_H

#include <cassert>
#include <ostream>
#include <caf/event_based_actor.hpp>
#include <caf/send.hpp>
#include <caf/to_string.hpp>

namespace caf {

inline std::ostream& operator<<(std::ostream& out, actor_addr const& a)
{
  out << '#' << a.id();
  return out;
}

inline std::ostream& operator<<(std::ostream& out, actor const& a)
{
  out << a.address();
  return out;
}

inline std::ostream& operator<<(std::ostream& out, abstract_actor const& a)
{
  out << a.address();
  return out;
}

template <typename Stream>
inline Stream& operator<<(Stream& out, actor const* a)
{
  assert(a != nullptr);
  out << *a;
  return out;
}

template <typename Stream>
inline Stream& operator<<(Stream& out, abstract_actor const* a)
{
  assert(a != nullptr);
  out << *a;
  return out;
}

template <typename Stream>
inline Stream& operator<<(Stream& out, event_based_actor const* a)
{
  assert(a != nullptr);
  out << *a;
  return out;
}

} // namespace caf

#include "vast/logger.h"
#include "vast/util/operators.h"
#include "vast/util/flat_set.h"

namespace vast {

namespace flow_control {

struct announce : util::equality_comparable<announce>
{
  announce(caf::actor a = {})
    : source{std::move(a)}
  {
  }

  caf::actor source;

  friend bool operator==(announce const& lhs, announce const& rhs)
  {
    return lhs.source == rhs.source;
  }
};

struct overload : util::equality_comparable<overload>
{
  friend bool operator==(overload const&, overload const&)
  {
    return true;
  }
};

struct underload : util::equality_comparable<underload>
{
  friend bool operator==(underload const&, underload const&)
  {
    return true;
  }
};

} // namespace flow_control

namespace exit {

constexpr uint32_t done   = caf::exit_reason::user_defined;
constexpr uint32_t stop   = caf::exit_reason::user_defined + 1;
constexpr uint32_t error  = caf::exit_reason::user_defined + 2;
constexpr uint32_t kill   = caf::exit_reason::user_defined + 3;

} // namespace exit

inline char const* render_exit_reason(uint32_t reason)
{
  switch (reason)
  {
    default:
      return "unknown";
    case exit::done:
      return "done";
    case exit::stop:
      return "stop";
    case exit::error:
      return "error";
    case exit::kill:
      return "kill";
    case caf::exit_reason::normal:
      return "normal";
    case caf::exit_reason::unhandled_exception:
      return "unhandled exception";
    case caf::exit_reason::unhandled_sync_failure:
      return "unhandled sync failure";
    case caf::exit_reason::unhandled_sync_timeout:
      return "unhandled sync timeout";
    case caf::exit_reason::user_shutdown:
      return "user shutdown";
    case caf::exit_reason::remote_link_unreachable:
      return "remote link unreachable";
  }
}

/// The base class for VAST actors.
class default_actor : public caf::event_based_actor
{
public:
  default_actor()
  {
    trap_exit(true);
    trap_unexpected(true);
  }

  caf::behavior make_behavior() override
  {
    VAST_DEBUG(this, "spawned");
    auto system = make_exit_handler().or_else(make_down_handler());
    auto internal = make_internal_handler();
    auto client = make_handler();
    auto handler = system.or_else(internal).or_else(client);
    if (trap_unexpected())
      handler = handler.or_else(make_unexpected_handler());
    return handler;
  }

  void on_exit()
  {
    VAST_DEBUG(this, "terminated (" <<
               render_exit_reason(planned_exit_reason()) << ')');
  }

  virtual std::string name() const
  {
    return "actor";
  }

  virtual std::string description() const
  {
    return name() + '#' + std::to_string(id());
  }

protected:
  virtual caf::message_handler make_handler() = 0;

  // For derived actors that need to inject a custom handler.
  virtual caf::message_handler make_internal_handler()
  {
    return {};
  }

  bool trap_unexpected()
  {
    return flags_ &= 0x01;
  }

  void trap_unexpected(bool flag)
  {
    if (flag)
      flags_ |= 0x01;
    else
      flags_ &= ~0x01;
  }

  virtual void at(caf::exit_msg const& msg)
  {
    quit(msg.reason);
  }

  virtual void at(caf::down_msg const&)
  {
    // Nothing by default.
  }

  virtual caf::message_handler make_exit_handler()
  {
    return [=](caf::exit_msg const& msg) { at(msg); };
  }

  virtual caf::message_handler make_down_handler()
  {
    return [=](caf::down_msg const& msg) { at(msg); };
  }

private:
  caf::message_handler make_unexpected_handler()
  {
    return
    {
      caf::others() >> [=]
      {
        VAST_WARN(this, "got unexpected message from",
                  last_sender() << ':', to_string(last_dequeued()));
      }
    };
  }

  // To keep actors as space-efficient as possible, we use this low-level
  // mechanism to customize the behavior.
  int flags_ = 0;
};

inline std::ostream& operator<<(std::ostream& out, default_actor const& a)
{
  out << a.description();
  return out;
}

template <typename Stream>
inline Stream& operator<<(Stream& out, default_actor const* a)
{
  assert(a != nullptr);
  out << *a;
  return out;
}

/// An actor which can participate in a flow-controlled setting.
class flow_controlled_actor : public default_actor
{
protected:
  // Hooks for clients.
  virtual void on_announce(caf::actor const&) { }
  virtual void on_overload() { }
  virtual void on_underload() { }

  caf::message_handler make_down_handler() final
  {
    return
      [=](caf::down_msg const& msg)
      {
        at(msg);
        VAST_DEBUG(this, "deregisters upstream flow-control node", msg.source);
        auto i = std::find_if(
            upstream_.begin(),
            upstream_.end(),
            [&](caf::actor const& a) { return a == msg.source; });
        if (i != upstream_.end())
          upstream_.erase(i);
      };
  }

  auto upstream() const
  {
    return upstream_;
  }

  void become_overloaded()
  {
    VAST_DEBUG(this, "becomes overloaded");
    become(
      caf::keep_behavior,
      [=](flow_control::overload)
      {
        VAST_DEBUG(this, "ignores overload signal");
      },
      [=](flow_control::underload)
      {
        VAST_DEBUG(this, "unbecomes overloaded");
        on_underload();
        unbecome();
      });
  }

  caf::message_handler make_internal_handler() final
  {
    using namespace caf;
    return
    {
      [=](flow_control::announce const& a)
      {
        assert(this != a.source);
        on_announce(a.source);
        VAST_DEBUG(this, "registers", a.source, "as upstream flow-control node");
        monitor(a.source);
        upstream_.insert(a.source);
      },
      [=](flow_control::overload)
      {
        on_overload();
        for (auto& a : upstream_)
        {
          VAST_DEBUG(this, "propagates overload signal to", a);
          send_tuple_as(this, a, message_priority::high, last_dequeued());
        }
      },
      [=](flow_control::underload)
      {
        on_underload();
        for (auto& a : upstream_)
        {
          VAST_DEBUG(this, "propagates underload signal to", a);
          send_tuple_as(this, a, message_priority::high, last_dequeued());
        }
      }
    };
  }

private:
  util::flat_set<caf::actor> upstream_;
};

} // namespace vast

#endif