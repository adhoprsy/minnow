#include <cstdint>
#include <iostream>

#include "arp_message.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  IPAddrNumeric next_hop_numeric = next_hop.ipv4_numeric();
  if ( arp_cache_.contains( next_hop_numeric ) ) {
    transmit( { {
                  arp_cache_[next_hop_numeric].ethaddr,
                  ethernet_address_,
                  EthernetHeader::TYPE_IPv4,
                },
                serialize( dgram ) } );
    return;
  }

  wait_to_send_[next_hop_numeric].emplace_back( dgram );
  if ( wait_retrans_timeout_.contains( next_hop_numeric ) )
    return;
  wait_retrans_timeout_.emplace( next_hop_numeric, Timer {} );
  transmit( { { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP },
              serialize( build_arp( ARPMessage::OPCODE_REQUEST, {}, next_hop_numeric ) ) } );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST )
    return;

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage dgram;
    if ( !parse( dgram, frame.payload ) )
      return;
    auto sender_ip = dgram.sender_ip_address;
    auto sender_eth = dgram.sender_ethernet_address;
    arp_cache_[sender_ip] = { sender_eth, Timer {} };
    if ( dgram.opcode == ARPMessage::OPCODE_REQUEST && dgram.target_ip_address == ip_address_.ipv4_numeric() ) {
      transmit( { { sender_eth, ethernet_address_, EthernetHeader::TYPE_ARP },
                  serialize( build_arp( ARPMessage::OPCODE_REPLY, sender_eth, sender_ip ) ) } );
    }

    if ( wait_to_send_.contains( sender_ip ) ) {
      for ( auto& dgram_to_send : wait_to_send_[sender_ip] ) {
        transmit( { { sender_eth, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize( dgram_to_send ) } );
      }
      wait_to_send_.erase( sender_ip );
      wait_retrans_timeout_.erase( sender_ip );
    }
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) )
      datagrams_received_.emplace( std::move( dgram ) );
    return;
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  // unordered_map does not invalidate other iter than the element erased
  std::vector<uint32_t> tmp;
  for ( auto& [k, v] : wait_retrans_timeout_ ) {
    if ( v.tick( ms_since_last_tick ).is_expired( ARP_RETRANS_TO ) )
      tmp.push_back( k );
  }
  for ( auto& k : tmp )
    wait_retrans_timeout_.erase( k );

  tmp.clear();

  for ( auto& [k, v] : arp_cache_ ) {
    if ( v.ttl.tick( ms_since_last_tick ).is_expired( ARP_ENRTY_TTL ) )
      tmp.push_back( k );
  }
  for ( auto& k : tmp )
    arp_cache_.erase( k );
}
