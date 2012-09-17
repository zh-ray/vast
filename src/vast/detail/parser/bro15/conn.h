#ifndef VAST_DETAIL_PARSER_BRO15_CONN_H
#define VAST_DETAIL_PARSER_BRO15_CONN_H

// This grammar has more struct fields than the default can handle.
#define FUSION_MAX_VECTOR_SIZE 20

// Improves compile times significantly at the cost of predefining terminals.
#define BOOST_SPIRIT_NO_PREDEFINED_TERMINALS

#include <ze/event.h>
#include <ze/detail/parser/address.h>

namespace vast {
namespace detail {
namespace parser {
namespace bro15 {

namespace qi = boost::spirit::qi;
namespace ascii = qi::ascii;

template <typename Iterator>
struct skipper : qi::grammar<Iterator>
{
  skipper()
    : skipper::base_type(start)
  {
    ascii::space_type space;

    start 
      = space - '\n'    // Tab & space
      ;
  };

  qi::rule<Iterator> start;
};

template <typename Iterator>
struct connection : qi::grammar<Iterator, ze::event(), qi::locals<uint16_t, uint16_t>, skipper<Iterator>>
{
  connection();

  qi::rule<Iterator, ze::event(), qi::locals<uint16_t, uint16_t>, skipper<Iterator>> conn;
  qi::rule<Iterator, ze::string()> id;
  qi::rule<Iterator, ze::string()> addl;

  ze::detail::parser::address<Iterator> addr;
};

} // namespace bro15
} // namespace parser
} // namespace detail
} // namespace vast

#endif
