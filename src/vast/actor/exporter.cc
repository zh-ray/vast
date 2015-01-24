#include "vast/actor/exporter.h"

#include <caf/all.hpp>
#include "vast/event.h"

using namespace caf;

namespace vast {

void exporter::at(down_msg const& msg)
{
  VAST_ERROR(this, "got DOWN from", msg.source);
  for (auto& s : sinks_)
    if (s == msg.source)
    {
      sinks_.erase(s);
      break;
    }

  if (sinks_.empty())
    quit(msg.reason);
}

message_handler exporter::make_handler()
{
  attach_functor(
      [=](uint32_t reason)
      {
        for (auto& s : sinks_)
          send_exit(s, reason);

        sinks_.clear();
      });

  return
  {
    on(atom("add"), arg_match) >> [=](actor const& snk)
    {
      monitor(snk);
      sinks_.insert(snk);
    },
    on(atom("limit"), arg_match) >> [=](uint64_t max)
    {
      VAST_DEBUG(this, "caps event export at", max, "events");

      if (processed_ < max)
        limit_ = max;
      else
        VAST_ERROR(this, "ignores new limit of", max <<
                   ", already processed", processed_, " events");
    },
    [=](event const&)
    {
      for (auto& s : sinks_)
        forward_to(s);

      if (++processed_ == limit_)
      {
        VAST_DEBUG(this, "reached maximum event limit:", limit_);
        quit(exit::done);
      }
    },
    on(atom("progress"), arg_match) >> [=](double progress, uint64_t hits)
    {
      VAST_DEBUG(this, "got query status message: completed ",
                 size_t(progress * 100) << "% (" << hits << " hits)");
    },
    on(atom("done")) >> [=]
    {
      VAST_DEBUG(this, "got query status message: done with index hits");
    }
  };
}

std::string exporter::name() const
{
  return "exporter";
}

} // namespace vast