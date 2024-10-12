#include "router.hh"
#include "ipv4_datagram.hh"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>

using namespace std;

void Router::TrieRouterTable::insert(RouterTableEntry entry) {
  size_t layer = 0;
  auto cur_node = root;
  while (layer < 4) {
    uint32_t mask_steps = (8*(0-layer));
    uint8_t ip_byte = static_cast<uint8_t>((entry.route_prefix >> mask_steps) & 0b1111);
    if (!nodes_[cur_node].edge.contains(ip_byte)) {
      nodes_.push_back({{}, nullopt});
      nodes_[cur_node].edge[ip_byte] = nodes_.size() - 1;
    }
    cur_node = nodes_[cur_node].edge[ip_byte];
    layer++;
  }
  nodes_[cur_node].entry.emplace(entry);
}

std::optional<Router::RouterTableEntry> Router::TrieRouterTable::query(const InternetDatagram& dgram) {
  size_t layer = 0;
  auto cur_node = root;
  std::optional<RouterTableEntry> matched{nullopt};

  while (layer < 4) {
    uint32_t mask_steps = (8*(0-layer));
    uint8_t ip_byte = static_cast<uint8_t>((dgram.header.dst >> mask_steps) & 0b1111);
    if (!nodes_[cur_node].edge.contains(ip_byte)) {
      break;
    }
    if (nodes_[cur_node].entry.has_value()) {
      matched.emplace(nodes_[cur_node].entry.value());
    }
    cur_node = nodes_[cur_node].edge[ip_byte];
    layer++;
  }
  return matched;
}
// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  _vector_router_table.emplace_back(  route_prefix, prefix_length, next_hop, interface_num  );
  _trie_router_table.insert({ route_prefix, prefix_length, next_hop, interface_num }); 
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here
  for ( const auto& interface : _interfaces ) {
    auto& dgram_queue { interface->datagrams_received() };
    while ( !dgram_queue.empty() ) {
      auto dgram { std::move( dgram_queue.front() ) };
      dgram_queue.pop();
      if ( dgram.header.ttl <= 1 )
        continue;
      dgram.header.ttl--;
      auto matched { plain_match( dgram ) };
      if ( !matched.has_value() )
        continue;

      _interfaces[matched.value().interface_idx]->send_datagram(
        dgram, matched.value().next_hop.value_or( Address::from_ipv4_numeric( dgram.header.dst ) ) );
    }
  }
}

std::optional<Router::RouterTableEntry> Router::plain_match( const InternetDatagram& dgram )
{
  std::optional<RouterTableEntry> max_matched {};
  for ( const auto& route_entry : _vector_router_table ) {
    if ( route_entry.prefix_length == 0
         || ( route_entry.route_prefix ^ dgram.header.dst ) >> ( 32 - route_entry.prefix_length ) == 0 ) {
      if ( !max_matched.has_value() || max_matched.value().prefix_length < route_entry.prefix_length )
        max_matched.emplace( route_entry );
    }
  }
  return max_matched;
}