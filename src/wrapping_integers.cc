#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { zero_point + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  const uint64_t MASK_LOW_32 = ( ( (uint64_t)1 << 32 ) - 1 );

  uint64_t n = static_cast<uint64_t>( raw_value_ - zero_point.raw_value_ );

  if ( checkpoint <= n )
    return n;

  uint64_t delta = checkpoint - n;
  return ( ( ( delta + ( MASK_LOW_32 >> 1 ) ) >> 32 ) << 32 ) + n;
}
