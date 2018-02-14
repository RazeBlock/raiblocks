#pragma once

#include <raze/lib/numbers.hpp>

#include <assert.h>
#include <blake2/blake2.h>
#include <boost/property_tree/json_parser.hpp>
#include <streambuf>

namespace raze
{
std::string to_string_hex (uint64_t);
bool from_string_hex (std::string const &, uint64_t &);
// We operate on streams of uint8_t by convention
using stream = std::basic_streambuf<uint8_t>;
// Read a raw byte stream the size of `T' and fill value.
template <typename T>
bool read (raze::stream & stream_a, T & value)
{
	static_assert (std::is_pod<T>::value, "Can't stream read non-standard layout types");
	auto amount_read (stream_a.sgetn (reinterpret_cast<uint8_t *> (&value), sizeof (value)));
	return amount_read != sizeof (value);
}
template <typename T>
void write (raze::stream & stream_a, T const & value)
{
	static_assert (std::is_pod<T>::value, "Can't stream write non-standard layout types");
	auto amount_written (stream_a.sputn (reinterpret_cast<uint8_t const *> (&value), sizeof (value)));
	assert (amount_written == sizeof (value));
}
class block_visitor;
enum class block_type : uint8_t
{
	invalid,
	not_a_block,
	send,
	receive,
	open,
	change
};
class block
{
public:
	// Return a digest of the hashables in this block.
	raze::block_hash hash () const;
	std::string to_json ();
	virtual void hash (blake2b_state &) const = 0;
	virtual uint64_t block_work () const = 0;
	virtual void block_work_set (uint64_t) = 0;
	// Previous block in account's chain, zero for open block
	virtual raze::block_hash previous () const = 0;
	// Source block for open/receive blocks, zero otherwise.
	virtual raze::block_hash source () const = 0;
	// Previous block or account number for open blocks
	virtual raze::block_hash root () const = 0;
	virtual raze::account representative () const = 0;
	virtual void serialize (raze::stream &) const = 0;
	virtual void serialize_json (std::string &) const = 0;
	virtual void visit (raze::block_visitor &) const = 0;
	virtual bool operator== (raze::block const &) const = 0;
	virtual raze::block_type type () const = 0;
	virtual void signature_set (raze::uint512_union const &) = 0;
	virtual ~block () = default;
};
class send_hashables
{
public:
	send_hashables (raze::account const &, raze::block_hash const &, raze::amount const &);
	send_hashables (bool &, raze::stream &);
	send_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	raze::block_hash previous;
	raze::account destination;
	raze::amount balance;
};
class send_block : public raze::block
{
public:
	send_block (raze::block_hash const &, raze::account const &, raze::amount const &, raze::raw_key const &, raze::public_key const &, uint64_t);
	send_block (bool &, raze::stream &);
	send_block (bool &, boost::property_tree::ptree const &);
	virtual ~send_block () = default;
	using raze::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	raze::block_hash previous () const override;
	raze::block_hash source () const override;
	raze::block_hash root () const override;
	raze::account representative () const override;
	void serialize (raze::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (raze::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (raze::block_visitor &) const override;
	raze::block_type type () const override;
	void signature_set (raze::uint512_union const &) override;
	bool operator== (raze::block const &) const override;
	bool operator== (raze::send_block const &) const;
	static size_t constexpr size = sizeof (raze::account) + sizeof (raze::block_hash) + sizeof (raze::amount) + sizeof (raze::signature) + sizeof (uint64_t);
	send_hashables hashables;
	raze::signature signature;
	uint64_t work;
};
class receive_hashables
{
public:
	receive_hashables (raze::block_hash const &, raze::block_hash const &);
	receive_hashables (bool &, raze::stream &);
	receive_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	raze::block_hash previous;
	raze::block_hash source;
};
class receive_block : public raze::block
{
public:
	receive_block (raze::block_hash const &, raze::block_hash const &, raze::raw_key const &, raze::public_key const &, uint64_t);
	receive_block (bool &, raze::stream &);
	receive_block (bool &, boost::property_tree::ptree const &);
	virtual ~receive_block () = default;
	using raze::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	raze::block_hash previous () const override;
	raze::block_hash source () const override;
	raze::block_hash root () const override;
	raze::account representative () const override;
	void serialize (raze::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (raze::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (raze::block_visitor &) const override;
	raze::block_type type () const override;
	void signature_set (raze::uint512_union const &) override;
	bool operator== (raze::block const &) const override;
	bool operator== (raze::receive_block const &) const;
	static size_t constexpr size = sizeof (raze::block_hash) + sizeof (raze::block_hash) + sizeof (raze::signature) + sizeof (uint64_t);
	receive_hashables hashables;
	raze::signature signature;
	uint64_t work;
};
class open_hashables
{
public:
	open_hashables (raze::block_hash const &, raze::account const &, raze::account const &);
	open_hashables (bool &, raze::stream &);
	open_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	raze::block_hash source;
	raze::account representative;
	raze::account account;
};
class open_block : public raze::block
{
public:
	open_block (raze::block_hash const &, raze::account const &, raze::account const &, raze::raw_key const &, raze::public_key const &, uint64_t);
	open_block (raze::block_hash const &, raze::account const &, raze::account const &, std::nullptr_t);
	open_block (bool &, raze::stream &);
	open_block (bool &, boost::property_tree::ptree const &);
	virtual ~open_block () = default;
	using raze::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	raze::block_hash previous () const override;
	raze::block_hash source () const override;
	raze::block_hash root () const override;
	raze::account representative () const override;
	void serialize (raze::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (raze::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (raze::block_visitor &) const override;
	raze::block_type type () const override;
	void signature_set (raze::uint512_union const &) override;
	bool operator== (raze::block const &) const override;
	bool operator== (raze::open_block const &) const;
	static size_t constexpr size = sizeof (raze::block_hash) + sizeof (raze::account) + sizeof (raze::account) + sizeof (raze::signature) + sizeof (uint64_t);
	raze::open_hashables hashables;
	raze::signature signature;
	uint64_t work;
};
class change_hashables
{
public:
	change_hashables (raze::block_hash const &, raze::account const &);
	change_hashables (bool &, raze::stream &);
	change_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	raze::block_hash previous;
	raze::account representative;
};
class change_block : public raze::block
{
public:
	change_block (raze::block_hash const &, raze::account const &, raze::raw_key const &, raze::public_key const &, uint64_t);
	change_block (bool &, raze::stream &);
	change_block (bool &, boost::property_tree::ptree const &);
	virtual ~change_block () = default;
	using raze::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	raze::block_hash previous () const override;
	raze::block_hash source () const override;
	raze::block_hash root () const override;
	raze::account representative () const override;
	void serialize (raze::stream &) const override;
	void serialize_json (std::string &) const override;
	bool deserialize (raze::stream &);
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (raze::block_visitor &) const override;
	raze::block_type type () const override;
	void signature_set (raze::uint512_union const &) override;
	bool operator== (raze::block const &) const override;
	bool operator== (raze::change_block const &) const;
	static size_t constexpr size = sizeof (raze::block_hash) + sizeof (raze::account) + sizeof (raze::signature) + sizeof (uint64_t);
	raze::change_hashables hashables;
	raze::signature signature;
	uint64_t work;
};
class block_visitor
{
public:
	virtual void send_block (raze::send_block const &) = 0;
	virtual void receive_block (raze::receive_block const &) = 0;
	virtual void open_block (raze::open_block const &) = 0;
	virtual void change_block (raze::change_block const &) = 0;
	virtual ~block_visitor () = default;
};
std::unique_ptr<raze::block> deserialize_block (raze::stream &);
std::unique_ptr<raze::block> deserialize_block (raze::stream &, raze::block_type);
std::unique_ptr<raze::block> deserialize_block_json (boost::property_tree::ptree const &);
void serialize_block (raze::stream &, raze::block const &);
}
