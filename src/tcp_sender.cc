#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <sys/types.h>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return numbers_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return consec_retransmission_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  uint64_t norm_window_size = window_size_ == 0 ? 1 : window_size_;
  while ( norm_window_size > numbers_in_flight_ ) {
    if ( state_ == AFTER_FIN )
      break;
    auto msg = make_empty_message();
    if ( state_ == STATE::BEFORE_SYN ) {
      msg.SYN = true;
      state_ = BETWEEN_SYN_FIN;
    }

    auto available_msg_len
      = min( TCPConfig::MAX_PAYLOAD_SIZE, norm_window_size - numbers_in_flight_ - msg.sequence_length() );

    string& payload { msg.payload };
    while ( reader().bytes_buffered() > 0 && payload.size() < available_msg_len ) {
      string_view v { reader().peek() };
      v = v.substr( 0, available_msg_len - payload.size() );
      payload += v;
      input_.reader().pop( v.size() );
    }

    if ( norm_window_size - numbers_in_flight_ > msg.sequence_length() && reader().is_finished() ) {
      msg.FIN = true;
      state_ = AFTER_FIN;
    }

    if ( msg.sequence_length() == 0 )
      break;

    next_abs_seqno_ += msg.sequence_length();
    numbers_in_flight_ += msg.sequence_length();

    transmit( msg );
    outstanding_.emplace_back( std::move( msg ) );
    if ( !timer_.is_alive() )
      timer_.start();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  return TCPSenderMessage { Wrap32::wrap( next_abs_seqno_, isn_ ), false, {}, false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  if ( input_.has_error() )
    return;
  if ( msg.RST ) {
    input_.set_error();
    return;
  }

  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() )
    return;

  uint64_t peer_ackno = msg.ackno.value().unwrap( isn_, next_abs_seqno_ );
  //
  if ( peer_ackno > next_abs_seqno_ )
    return;

  bool has_ack_msg = false;
  while ( !outstanding_.empty() ) {
    auto& front { outstanding_.front() };
    // this segment is not fully acked
    if ( acked_abs_seqno_ + front.sequence_length() > peer_ackno )
      break;

    has_ack_msg = true;
    acked_abs_seqno_ += front.sequence_length();
    numbers_in_flight_ -= front.sequence_length();
    outstanding_.pop_front();
  }

  if ( has_ack_msg ) {
    timer_.set( initial_RTO_ms_ );
    consec_retransmission_ = 0;
    outstanding_.empty() ? timer_.stop() : timer_.start();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  if ( !timer_.is_alive() )
    return;
  if ( timer_.tick( ms_since_last_tick ).is_expired() ) {
    if ( outstanding_.empty() )
      return;
    transmit( outstanding_.front() );

    if ( window_size_ > 0 ) {
      consec_retransmission_++;
      timer_.exp_backoff();
    }

    timer_.reset();
  }
}
