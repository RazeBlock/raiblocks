#pragma once

#include <raze/lib/interface.h>
#include <raze/secure.hpp>

#include <boost/asio.hpp>

#include <bitset>

#include <xxhash/xxhash.h>

namespace raze
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, raze::endpoint &);
bool parse_tcp_endpoint (std::string const &, raze::tcp_endpoint &);
bool reserved_address (raze::endpoint const &);
}
static uint64_t endpoint_hash_raw (raze::endpoint const & endpoint_a)
{
	assert (endpoint_a.address ().is_v6 ());
	raze::uint128_union address;
	address.bytes = endpoint_a.address ().to_v6 ().to_bytes ();
	XXH64_state_t hash;
	XXH64_reset (&hash, 0);
	XXH64_update (&hash, address.bytes.data (), address.bytes.size ());
	auto port (endpoint_a.port ());
	XXH64_update (&hash, &port, sizeof (port));
	auto result (XXH64_digest (&hash));
	return result;
}

namespace std
{
template <size_t size>
struct endpoint_hash
{
};
template <>
struct endpoint_hash<8>
{
	size_t operator() (raze::endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
};
template <>
struct endpoint_hash<4>
{
	size_t operator() (raze::endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
template <>
struct hash<raze::endpoint>
{
	size_t operator() (raze::endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (size_t)> ehash;
		return ehash (endpoint_a);
	}
};
}
namespace boost
{
template <>
struct hash<raze::endpoint>
{
	size_t operator() (raze::endpoint const & endpoint_a) const
	{
		std::hash<raze::endpoint> hash;
		return hash (endpoint_a);
	}
};
}

namespace raze
{
enum class message_type : uint8_t
{
	invalid,
	not_a_type,
	keepalive,
	publish,
	confirm_req,
	confirm_ack,
	bulk_pull,
	bulk_push,
	frontier_req,
	bulk_pull_blocks
};
enum class bulk_pull_blocks_mode : uint8_t
{
	list_blocks,
	checksum_blocks
};
class message_visitor;
class message
{
public:
	message (raze::message_type);
	message (bool &, raze::stream &);
	virtual ~message () = default;
	void write_header (raze::stream &);
	static bool read_header (raze::stream &, uint8_t &, uint8_t &, uint8_t &, raze::message_type &, std::bitset<16> &);
	virtual void serialize (raze::stream &) = 0;
	virtual bool deserialize (raze::stream &) = 0;
	virtual void visit (raze::message_visitor &) const = 0;
	raze::block_type block_type () const;
	void block_type_set (raze::block_type);
	bool ipv4_only ();
	void ipv4_only_set (bool);
	static std::array<uint8_t, 2> constexpr magic_number = raze::raze_network == raze::raze_networks::raze_test_network ? std::array<uint8_t, 2> ({ 'R', 'A' }) : raze::raze_network == raze::raze_networks::raze_beta_network ? std::array<uint8_t, 2> ({ 'R', 'B' }) : std::array<uint8_t, 2> ({ 'R', 'C' });
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	raze::message_type type;
	std::bitset<16> extensions;
	static size_t constexpr ipv4_only_position = 1;
	static size_t constexpr bootstrap_server_position = 2;
	static std::bitset<16> constexpr block_type_mask = std::bitset<16> (0x0f00);
};
class work_pool;
class message_parser
{
public:
	message_parser (raze::message_visitor &, raze::work_pool &);
	void deserialize_buffer (uint8_t const *, size_t);
	void deserialize_keepalive (uint8_t const *, size_t);
	void deserialize_publish (uint8_t const *, size_t);
	void deserialize_confirm_req (uint8_t const *, size_t);
	void deserialize_confirm_ack (uint8_t const *, size_t);
	bool at_end (raze::bufferstream &);
	raze::message_visitor & visitor;
	raze::work_pool & pool;
	bool error;
	bool insufficient_work;
};
class keepalive : public message
{
public:
	keepalive ();
	void visit (raze::message_visitor &) const override;
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	bool operator== (raze::keepalive const &) const;
	std::array<raze::endpoint, 8> peers;
};
class publish : public message
{
public:
	publish ();
	publish (std::shared_ptr<raze::block>);
	void visit (raze::message_visitor &) const override;
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	bool operator== (raze::publish const &) const;
	std::shared_ptr<raze::block> block;
};
class confirm_req : public message
{
public:
	confirm_req ();
	confirm_req (std::shared_ptr<raze::block>);
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	void visit (raze::message_visitor &) const override;
	bool operator== (raze::confirm_req const &) const;
	std::shared_ptr<raze::block> block;
};
class confirm_ack : public message
{
public:
	confirm_ack (bool &, raze::stream &);
	confirm_ack (std::shared_ptr<raze::vote>);
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	void visit (raze::message_visitor &) const override;
	bool operator== (raze::confirm_ack const &) const;
	std::shared_ptr<raze::vote> vote;
};
class frontier_req : public message
{
public:
	frontier_req ();
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	void visit (raze::message_visitor &) const override;
	bool operator== (raze::frontier_req const &) const;
	raze::account start;
	uint32_t age;
	uint32_t count;
};
class bulk_pull : public message
{
public:
	bulk_pull ();
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	void visit (raze::message_visitor &) const override;
	raze::uint256_union start;
	raze::block_hash end;
};
class bulk_pull_blocks : public message
{
public:
	bulk_pull_blocks ();
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	void visit (raze::message_visitor &) const override;
	raze::block_hash min_hash;
	raze::block_hash max_hash;
	bulk_pull_blocks_mode mode;
	uint32_t max_count;
};
class bulk_push : public message
{
public:
	bulk_push ();
	bool deserialize (raze::stream &) override;
	void serialize (raze::stream &) override;
	void visit (raze::message_visitor &) const override;
};
class message_visitor
{
public:
	virtual void keepalive (raze::keepalive const &) = 0;
	virtual void publish (raze::publish const &) = 0;
	virtual void confirm_req (raze::confirm_req const &) = 0;
	virtual void confirm_ack (raze::confirm_ack const &) = 0;
	virtual void bulk_pull (raze::bulk_pull const &) = 0;
	virtual void bulk_pull_blocks (raze::bulk_pull_blocks const &) = 0;
	virtual void bulk_push (raze::bulk_push const &) = 0;
	virtual void frontier_req (raze::frontier_req const &) = 0;
	virtual ~message_visitor ();
};

/**
 * Returns seconds passed since unix epoch (posix time)
 */
inline uint64_t seconds_since_epoch ()
{
	return std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
}
}
