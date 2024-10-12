#pragma once

#include <memory>
#include <optional>

#include "exception.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};

  struct RouterTableEntry
  {
    const uint32_t route_prefix;
    const uint8_t prefix_length;
    const std::optional<Address> next_hop;
    const size_t interface_idx;
  };
  std::vector<RouterTableEntry> _vector_router_table {};

  std::optional<Router::RouterTableEntry> plain_match( const InternetDatagram& );


  struct TrieNode {
    std::unordered_map<uint8_t, size_t> edge;
    std::optional<RouterTableEntry> entry;
  };
  struct TrieRouterTable {
    std::vector<TrieNode> nodes_{{{},std::nullopt}};
    size_t root{0};

    void insert(RouterTableEntry);
    std::optional<RouterTableEntry> query(const InternetDatagram&);
  };
  TrieRouterTable _trie_router_table{};
};
