#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <sys/types.h>

struct Timer
{
  explicit Timer( uint64_t TO ) : RTO_( TO ) {}

  bool is_alive() { return alive_; }
  bool is_expired() { return time_ >= RTO_; }
  void start()
  {
    alive_ = true;
    reset();
  }
  void stop()
  {
    alive_ = false;
    reset();
  }
  void set( uint64_t new_TO )
  {
    RTO_ = new_TO;
    reset();
  }
  void reset() { time_ = 0; }
  void exp_backoff() { RTO_ <<= 1; }
  Timer& tick( uint64_t time_passed )
  {
    time_ += time_passed;
    return *this;
  }

private:
  bool alive_ {};
  uint64_t RTO_;
  uint64_t time_ {};
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  Timer timer_;
  uint64_t numbers_in_flight_ {};
  uint64_t consec_retransmission_ {};

  std::deque<TCPSenderMessage> outstanding_ {};
  uint64_t window_size_ { 1 };
  uint64_t next_abs_seqno_ {};
  uint64_t acked_abs_seqno_ {}; // syn is [0], so init with 0 is ok

  enum STATE
  {
    BEFORE_SYN,
    BETWEEN_SYN_FIN,
    AFTER_FIN,
  };
  STATE state_ { BEFORE_SYN };
};
