#include <raze/node/common.hpp>

#include <raze/lib/work.hpp>
#include <raze/node/wallet.hpp>

std::array<uint8_t, 2> constexpr raze::message::magic_number;
size_t constexpr raze::message::ipv4_only_position;
size_t constexpr raze::message::bootstrap_server_position;
std::bitset<16> constexpr raze::message::block_type_mask;

raze::message::message (raze::message_type type_a) :
version_max (0x06),
version_using (0x06),
version_min (0x01),
type (type_a)
{
}

raze::message::message (bool & error_a, raze::stream & stream_a)
{
	error_a = read_header (stream_a, version_max, version_using, version_min, type, extensions);
}

raze::block_type raze::message::block_type () const
{
	return static_cast<raze::block_type> (((extensions & block_type_mask) >> 8).to_ullong ());
}

void raze::message::block_type_set (raze::block_type type_a)
{
	extensions &= ~raze::message::block_type_mask;
	extensions |= std::bitset<16> (static_cast<unsigned long long> (type_a) << 8);
}

bool raze::message::ipv4_only ()
{
	return extensions.test (ipv4_only_position);
}

void raze::message::ipv4_only_set (bool value_a)
{
	extensions.set (ipv4_only_position, value_a);
}

void raze::message::write_header (raze::stream & stream_a)
{
	raze::write (stream_a, raze::message::magic_number);
	raze::write (stream_a, version_max);
	raze::write (stream_a, version_using);
	raze::write (stream_a, version_min);
	raze::write (stream_a, type);
	raze::write (stream_a, static_cast<uint16_t> (extensions.to_ullong ()));
}

bool raze::message::read_header (raze::stream & stream_a, uint8_t & version_max_a, uint8_t & version_using_a, uint8_t & version_min_a, raze::message_type & type_a, std::bitset<16> & extensions_a)
{
	uint16_t extensions_l;
	std::array<uint8_t, 2> magic_number_l;
	auto result (raze::read (stream_a, magic_number_l));
	result = result || magic_number_l != magic_number;
	result = result || raze::read (stream_a, version_max_a);
	result = result || raze::read (stream_a, version_using_a);
	result = result || raze::read (stream_a, version_min_a);
	result = result || raze::read (stream_a, type_a);
	result = result || raze::read (stream_a, extensions_l);
	if (!result)
	{
		extensions_a = extensions_l;
	}
	return result;
}

raze::message_parser::message_parser (raze::message_visitor & visitor_a, raze::work_pool & pool_a) :
visitor (visitor_a),
pool (pool_a),
error (false),
insufficient_work (false)
{
}

void raze::message_parser::deserialize_buffer (uint8_t const * buffer_a, size_t size_a)
{
	error = false;
	raze::bufferstream header_stream (buffer_a, size_a);
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	raze::message_type type;
	std::bitset<16> extensions;
	if (!raze::message::read_header (header_stream, version_max, version_using, version_min, type, extensions))
	{
		switch (type)
		{
			case raze::message_type::keepalive:
			{
				deserialize_keepalive (buffer_a, size_a);
				break;
			}
			case raze::message_type::publish:
			{
				deserialize_publish (buffer_a, size_a);
				break;
			}
			case raze::message_type::confirm_req:
			{
				deserialize_confirm_req (buffer_a, size_a);
				break;
			}
			case raze::message_type::confirm_ack:
			{
				deserialize_confirm_ack (buffer_a, size_a);
				break;
			}
			default:
			{
				error = true;
				break;
			}
		}
	}
	else
	{
		error = true;
	}
}

void raze::message_parser::deserialize_keepalive (uint8_t const * buffer_a, size_t size_a)
{
	raze::keepalive incoming;
	raze::bufferstream stream (buffer_a, size_a);
	auto error_l (incoming.deserialize (stream));
	if (!error_l && at_end (stream))
	{
		visitor.keepalive (incoming);
	}
	else
	{
		error = true;
	}
}

void raze::message_parser::deserialize_publish (uint8_t const * buffer_a, size_t size_a)
{
	raze::publish incoming;
	raze::bufferstream stream (buffer_a, size_a);
	auto error_l (incoming.deserialize (stream));
	if (!error_l && at_end (stream))
	{
		if (!raze::work_validate (*incoming.block))
		{
			visitor.publish (incoming);
		}
		else
		{
			insufficient_work = true;
		}
	}
	else
	{
		error = true;
	}
}

void raze::message_parser::deserialize_confirm_req (uint8_t const * buffer_a, size_t size_a)
{
	raze::confirm_req incoming;
	raze::bufferstream stream (buffer_a, size_a);
	auto error_l (incoming.deserialize (stream));
	if (!error_l && at_end (stream))
	{
		if (!raze::work_validate (*incoming.block))
		{
			visitor.confirm_req (incoming);
		}
		else
		{
			insufficient_work = true;
		}
	}
	else
	{
		error = true;
	}
}

void raze::message_parser::deserialize_confirm_ack (uint8_t const * buffer_a, size_t size_a)
{
	bool error_l;
	raze::bufferstream stream (buffer_a, size_a);
	raze::confirm_ack incoming (error_l, stream);
	if (!error_l && at_end (stream))
	{
		if (!raze::work_validate (*incoming.vote->block))
		{
			visitor.confirm_ack (incoming);
		}
		else
		{
			insufficient_work = true;
		}
	}
	else
	{
		error = true;
	}
}

bool raze::message_parser::at_end (raze::bufferstream & stream_a)
{
	uint8_t junk;
	auto end (raze::read (stream_a, junk));
	return end;
}

raze::keepalive::keepalive () :
message (raze::message_type::keepalive)
{
	raze::endpoint endpoint (boost::asio::ip::address_v6{}, 0);
	for (auto i (peers.begin ()), n (peers.end ()); i != n; ++i)
	{
		*i = endpoint;
	}
}

void raze::keepalive::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.keepalive (*this);
}

void raze::keepalive::serialize (raze::stream & stream_a)
{
	write_header (stream_a);
	for (auto i (peers.begin ()), j (peers.end ()); i != j; ++i)
	{
		assert (i->address ().is_v6 ());
		auto bytes (i->address ().to_v6 ().to_bytes ());
		write (stream_a, bytes);
		write (stream_a, i->port ());
	}
}

bool raze::keepalive::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == raze::message_type::keepalive);
	for (auto i (peers.begin ()), j (peers.end ()); i != j; ++i)
	{
		std::array<uint8_t, 16> address;
		uint16_t port;
		read (stream_a, address);
		read (stream_a, port);
		*i = raze::endpoint (boost::asio::ip::address_v6 (address), port);
	}
	return result;
}

bool raze::keepalive::operator== (raze::keepalive const & other_a) const
{
	return peers == other_a.peers;
}

raze::publish::publish () :
message (raze::message_type::publish)
{
}

raze::publish::publish (std::shared_ptr<raze::block> block_a) :
message (raze::message_type::publish),
block (block_a)
{
	block_type_set (block->type ());
}

bool raze::publish::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == raze::message_type::publish);
	if (!result)
	{
		block = raze::deserialize_block (stream_a, block_type ());
		result = block == nullptr;
	}
	return result;
}

void raze::publish::serialize (raze::stream & stream_a)
{
	assert (block != nullptr);
	write_header (stream_a);
	block->serialize (stream_a);
}

void raze::publish::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.publish (*this);
}

bool raze::publish::operator== (raze::publish const & other_a) const
{
	return *block == *other_a.block;
}

raze::confirm_req::confirm_req () :
message (raze::message_type::confirm_req)
{
}

raze::confirm_req::confirm_req (std::shared_ptr<raze::block> block_a) :
message (raze::message_type::confirm_req),
block (block_a)
{
	block_type_set (block->type ());
}

bool raze::confirm_req::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == raze::message_type::confirm_req);
	if (!result)
	{
		block = raze::deserialize_block (stream_a, block_type ());
		result = block == nullptr;
	}
	return result;
}

void raze::confirm_req::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.confirm_req (*this);
}

void raze::confirm_req::serialize (raze::stream & stream_a)
{
	assert (block != nullptr);
	write_header (stream_a);
	block->serialize (stream_a);
}

bool raze::confirm_req::operator== (raze::confirm_req const & other_a) const
{
	return *block == *other_a.block;
}

raze::confirm_ack::confirm_ack (bool & error_a, raze::stream & stream_a) :
message (error_a, stream_a),
vote (std::make_shared<raze::vote> (error_a, stream_a, block_type ()))
{
}

raze::confirm_ack::confirm_ack (std::shared_ptr<raze::vote> vote_a) :
message (raze::message_type::confirm_ack),
vote (vote_a)
{
	block_type_set (vote->block->type ());
}

bool raze::confirm_ack::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (type == raze::message_type::confirm_ack);
	if (!result)
	{
		result = read (stream_a, vote->account);
		if (!result)
		{
			result = read (stream_a, vote->signature);
			if (!result)
			{
				result = read (stream_a, vote->sequence);
				if (!result)
				{
					vote->block = raze::deserialize_block (stream_a, block_type ());
					result = vote->block == nullptr;
				}
			}
		}
	}
	return result;
}

void raze::confirm_ack::serialize (raze::stream & stream_a)
{
	assert (block_type () == raze::block_type::send || block_type () == raze::block_type::receive || block_type () == raze::block_type::open || block_type () == raze::block_type::change);
	write_header (stream_a);
	vote->serialize (stream_a, block_type ());
}

bool raze::confirm_ack::operator== (raze::confirm_ack const & other_a) const
{
	auto result (*vote == *other_a.vote);
	return result;
}

void raze::confirm_ack::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.confirm_ack (*this);
}

raze::frontier_req::frontier_req () :
message (raze::message_type::frontier_req)
{
}

bool raze::frontier_req::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (raze::message_type::frontier_req == type);
	if (!result)
	{
		assert (type == raze::message_type::frontier_req);
		result = read (stream_a, start.bytes);
		if (!result)
		{
			result = read (stream_a, age);
			if (!result)
			{
				result = read (stream_a, count);
			}
		}
	}
	return result;
}

void raze::frontier_req::serialize (raze::stream & stream_a)
{
	write_header (stream_a);
	write (stream_a, start.bytes);
	write (stream_a, age);
	write (stream_a, count);
}

void raze::frontier_req::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.frontier_req (*this);
}

bool raze::frontier_req::operator== (raze::frontier_req const & other_a) const
{
	return start == other_a.start && age == other_a.age && count == other_a.count;
}

raze::bulk_pull::bulk_pull () :
message (raze::message_type::bulk_pull)
{
}

void raze::bulk_pull::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull (*this);
}

bool raze::bulk_pull::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (raze::message_type::bulk_pull == type);
	if (!result)
	{
		assert (type == raze::message_type::bulk_pull);
		result = read (stream_a, start);
		if (!result)
		{
			result = read (stream_a, end);
		}
	}
	return result;
}

void raze::bulk_pull::serialize (raze::stream & stream_a)
{
	write_header (stream_a);
	write (stream_a, start);
	write (stream_a, end);
}

raze::bulk_pull_blocks::bulk_pull_blocks () :
message (raze::message_type::bulk_pull_blocks)
{
}

void raze::bulk_pull_blocks::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.bulk_pull_blocks (*this);
}

bool raze::bulk_pull_blocks::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (raze::message_type::bulk_pull_blocks == type);
	if (!result)
	{
		assert (type == raze::message_type::bulk_pull_blocks);
		result = read (stream_a, min_hash);
		if (!result)
		{
			result = read (stream_a, max_hash);
		}

		if (!result)
		{
			result = read (stream_a, mode);
		}

		if (!result)
		{
			result = read (stream_a, max_count);
		}
	}
	return result;
}

void raze::bulk_pull_blocks::serialize (raze::stream & stream_a)
{
	write_header (stream_a);
	write (stream_a, min_hash);
	write (stream_a, max_hash);
	write (stream_a, mode);
	write (stream_a, max_count);
}

raze::bulk_push::bulk_push () :
message (raze::message_type::bulk_push)
{
}

bool raze::bulk_push::deserialize (raze::stream & stream_a)
{
	auto result (read_header (stream_a, version_max, version_using, version_min, type, extensions));
	assert (!result);
	assert (raze::message_type::bulk_push == type);
	return result;
}

void raze::bulk_push::serialize (raze::stream & stream_a)
{
	write_header (stream_a);
}

void raze::bulk_push::visit (raze::message_visitor & visitor_a) const
{
	visitor_a.bulk_push (*this);
}

raze::message_visitor::~message_visitor ()
{
}
