#include "tcp_receiver.hh"
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if ( writer().has_error() )
    return;
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( !message.SYN && !isn_.has_value() )
    return;

  if ( message.SYN && !isn_.has_value() )
    isn_.emplace( message.seqno );

  // convert seqno to uint64_t streamindex
  uint64_t checkpoint = writer().bytes_pushed() + 1; // seqno includes syn
  uint64_t abs_seqno = message.seqno.unwrap( isn_.value(), checkpoint );
  uint64_t stream_index;
  if ( !message.SYN )
    stream_index = abs_seqno - 1;
  else
    stream_index = abs_seqno;

  reassembler_.insert( stream_index, std::move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  std::optional<Wrap32> ackno { nullopt };
  if ( isn_.has_value() ) {
    if ( writer().is_closed() )
      ackno.emplace( Wrap32( isn_.value() ) + writer().bytes_pushed() + 2 );
    else
      ackno.emplace( Wrap32( isn_.value() ) + writer().bytes_pushed() + 1 );
  }
  uint16_t window_size
    = static_cast<uint16_t>( min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );

  return { ackno, window_size, writer().has_error() };
}
