#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <ranges>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // Your code here.
  if ( writer().is_closed() || writer().available_capacity() == 0 )
    return;

  // [discard_index, ..)
  const uint64_t discard_index = nextIndex + writer().available_capacity();
  // don't forget starts from zero
  if ( first_index >= discard_index || first_index + data.size() < nextIndex ) {
    return;
  }
  // trimming
  if ( first_index + data.size() > discard_index ) {
    data.resize( discard_index - first_index );
    is_last_substring = false;
  }
  if ( first_index < nextIndex ) {
    data.erase( 0, nextIndex - first_index );
    first_index = nextIndex;
  }

  if ( !maxBuffered_.has_value() && is_last_substring )
    maxBuffered_.emplace( first_index + data.size() );
  //
  auto lower = buffer_.split( first_index );
  auto upper = buffer_.split( first_index + data.size() );
  ranges::for_each( ranges::subrange( lower, upper ) | views::values,
                    [&]( const auto& str ) { pending_ -= str.size(); } );

  pending_ += data.size();
  auto hint = buffer_.inner_.erase( lower, upper );
  buffer_.inner_.emplace_hint( hint, first_index, std::move( data ) );

  // send prefix to writer
  while ( !buffer_.empty() ) {
    auto& [i, s] = *buffer_.inner_.begin();
    if ( i != nextIndex )
      break;

    pending_ -= s.size();
    output_.writer().push( std::move( s ) );
    nextIndex = writer().bytes_pushed();
    buffer_.inner_.erase( buffer_.inner_.begin() );
  }

  if ( maxBuffered_.has_value() && maxBuffered_.value() == writer().bytes_pushed() ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return pending_;
}
