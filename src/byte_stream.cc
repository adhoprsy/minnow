#include "byte_stream.hh"
#include <cstdint>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

void Writer::push( string data )
{
  // Your code here.
  if ( is_closed() || available_capacity() == 0 || data.empty() )
    return;
  uint64_t length = data.size();
  uint64_t len_to_push = min( length, capacity_ - buffered_ );
  data.resize( len_to_push );
  buffer_.emplace_back( move( data ) );
  pushed_ += len_to_push;
  buffered_ += len_to_push;
  return;
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - buffered_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return pushed_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return buffer_.empty() && closed_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return poped_;
}

string_view Reader::peek() const
{
  // Your code here.
  if ( bytes_buffered() == 0 )
    return string_view {};
  return string_view( buffer_.front() ).substr( prefix_poped_ );
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  len = min( len, buffered_ );
  while ( len > 0 ) {
    uint64_t tmplen = 0;
    if ( buffer_.front().size() - prefix_poped_ <= len ) {
      tmplen = buffer_.front().size() - prefix_poped_;
      prefix_poped_ = 0;
      buffer_.pop_front();
    } else {
      tmplen = len;
      prefix_poped_ += len;
    }
    len -= tmplen;
    poped_ += tmplen;
    buffered_ -= tmplen;
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buffered_;
}
