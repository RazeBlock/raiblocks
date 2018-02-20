#include <raze/lib/blocks.hpp>

std::string raze::to_string_hex (uint64_t value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}

bool raze::from_string_hex (std::string const & value_a, uint64_t & target_a)
{
	auto error (value_a.empty ());
	if (!error)
	{
		error = value_a.size () > 16;
		if (!error)
		{
			std::stringstream stream (value_a);
			stream << std::hex << std::noshowbase;
			try
			{
				uint64_t number_l;
				stream >> number_l;
				target_a = number_l;
				if (!stream.eof ())
				{
					error = true;
				}
			}
			catch (std::runtime_error &)
			{
				error = true;
			}
		}
	}
	return error;
}

std::string raze::block::to_json ()
{
	std::string result;
	serialize_json (result);
	return result;
}

raze::block_hash raze::block::hash () const
{
	raze::uint256_union result;
	blake2b_state hash_l;
	auto status (blake2b_init (&hash_l, sizeof (result.bytes)));
	assert (status == 0);
	hash (hash_l);
	status = blake2b_final (&hash_l, result.bytes.data (), sizeof (result.bytes));
	assert (status == 0);
	return result;
}

void raze::send_block::visit (raze::block_visitor & visitor_a) const
{
	visitor_a.send_block (*this);
}

void raze::send_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t raze::send_block::block_work () const
{
	return work;
}

void raze::send_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

raze::send_hashables::send_hashables (raze::block_hash const & previous_a, raze::account const & destination_a, raze::amount const & balance_a) :
previous (previous_a),
destination (destination_a),
balance (balance_a)
{
}

raze::send_hashables::send_hashables (bool & error_a, raze::stream & stream_a)
{
	error_a = raze::read (stream_a, previous.bytes);
	if (!error_a)
	{
		error_a = raze::read (stream_a, destination.bytes);
		if (!error_a)
		{
			error_a = raze::read (stream_a, balance.bytes);
		}
	}
}

raze::send_hashables::send_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = destination.decode_account (destination_l);
			if (!error_a)
			{
				error_a = balance.decode_hex (balance_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void raze::send_hashables::hash (blake2b_state & hash_a) const
{
	auto status (blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes)));
	assert (status == 0);
	status = blake2b_update (&hash_a, destination.bytes.data (), sizeof (destination.bytes));
	assert (status == 0);
	status = blake2b_update (&hash_a, balance.bytes.data (), sizeof (balance.bytes));
	assert (status == 0);
}

void raze::send_block::serialize (raze::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.destination.bytes);
	write (stream_a, hashables.balance.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

void raze::send_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "send");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	tree.put ("destination", hashables.destination.to_account ());
	std::string balance;
	hashables.balance.encode_hex (balance);
	tree.put ("balance", balance);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", raze::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool raze::send_block::deserialize (raze::stream & stream_a)
{
	auto error (false);
	error = read (stream_a, hashables.previous.bytes);
	if (!error)
	{
		error = read (stream_a, hashables.destination.bytes);
		if (!error)
		{
			error = read (stream_a, hashables.balance.bytes);
			if (!error)
			{
				error = read (stream_a, signature.bytes);
				if (!error)
				{
					error = read (stream_a, work);
				}
			}
		}
	}
	return error;
}

bool raze::send_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "send");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.destination.decode_account (destination_l);
			if (!error)
			{
				error = hashables.balance.decode_hex (balance_l);
				if (!error)
				{
					error = raze::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

raze::send_block::send_block (raze::block_hash const & previous_a, raze::account const & destination_a, raze::amount const & balance_a, raze::raw_key const & prv_a, raze::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, destination_a, balance_a),
signature (raze::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

raze::send_block::send_block (bool & error_a, raze::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = raze::read (stream_a, signature.bytes);
		if (!error_a)
		{
			error_a = raze::read (stream_a, work);
		}
	}
}

raze::send_block::send_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = raze::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

bool raze::send_block::operator== (raze::block const & other_a) const
{
	auto other_l (dynamic_cast<raze::send_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

raze::block_type raze::send_block::type () const
{
	return raze::block_type::send;
}

bool raze::send_block::operator== (raze::send_block const & other_a) const
{
	auto result (hashables.destination == other_a.hashables.destination && hashables.previous == other_a.hashables.previous && hashables.balance == other_a.hashables.balance && work == other_a.work && signature == other_a.signature);
	return result;
}

raze::block_hash raze::send_block::previous () const
{
	return hashables.previous;
}

raze::block_hash raze::send_block::source () const
{
	return 0;
}

raze::block_hash raze::send_block::root () const
{
	return hashables.previous;
}

raze::account raze::send_block::representative () const
{
	return 0;
}

raze::signature raze::send_block::block_signature () const
{
	return signature;
}

void raze::send_block::signature_set (raze::uint512_union const & signature_a)
{
	signature = signature_a;
}

raze::open_hashables::open_hashables (raze::block_hash const & source_a, raze::account const & representative_a, raze::account const & account_a) :
source (source_a),
representative (representative_a),
account (account_a)
{
}

raze::open_hashables::open_hashables (bool & error_a, raze::stream & stream_a)
{
	error_a = raze::read (stream_a, source.bytes);
	if (!error_a)
	{
		error_a = raze::read (stream_a, representative.bytes);
		if (!error_a)
		{
			error_a = raze::read (stream_a, account.bytes);
		}
	}
}

raze::open_hashables::open_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		error_a = source.decode_hex (source_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
			if (!error_a)
			{
				error_a = account.decode_account (account_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void raze::open_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
}

raze::open_block::open_block (raze::block_hash const & source_a, raze::account const & representative_a, raze::account const & account_a, raze::raw_key const & prv_a, raze::public_key const & pub_a, uint64_t work_a) :
hashables (source_a, representative_a, account_a),
signature (raze::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
	assert (!representative_a.is_zero ());
}

raze::open_block::open_block (raze::block_hash const & source_a, raze::account const & representative_a, raze::account const & account_a, std::nullptr_t) :
hashables (source_a, representative_a, account_a),
work (0)
{
	signature.clear ();
}

raze::open_block::open_block (bool & error_a, raze::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = raze::read (stream_a, signature);
		if (!error_a)
		{
			error_a = raze::read (stream_a, work);
		}
	}
}

raze::open_block::open_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = raze::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void raze::open_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t raze::open_block::block_work () const
{
	return work;
}

void raze::open_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

raze::block_hash raze::open_block::previous () const
{
	raze::block_hash result (0);
	return result;
}

void raze::open_block::serialize (raze::stream & stream_a) const
{
	write (stream_a, hashables.source);
	write (stream_a, hashables.representative);
	write (stream_a, hashables.account);
	write (stream_a, signature);
	write (stream_a, work);
}

void raze::open_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "open");
	tree.put ("source", hashables.source.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("account", hashables.account.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", raze::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool raze::open_block::deserialize (raze::stream & stream_a)
{
	auto error (read (stream_a, hashables.source));
	if (!error)
	{
		error = read (stream_a, hashables.representative);
		if (!error)
		{
			error = read (stream_a, hashables.account);
			if (!error)
			{
				error = read (stream_a, signature);
				if (!error)
				{
					error = read (stream_a, work);
				}
			}
		}
	}
	return error;
}

bool raze::open_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "open");
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.source.decode_hex (source_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = hashables.account.decode_hex (account_l);
				if (!error)
				{
					error = raze::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void raze::open_block::visit (raze::block_visitor & visitor_a) const
{
	visitor_a.open_block (*this);
}

raze::block_type raze::open_block::type () const
{
	return raze::block_type::open;
}

bool raze::open_block::operator== (raze::block const & other_a) const
{
	auto other_l (dynamic_cast<raze::open_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

bool raze::open_block::operator== (raze::open_block const & other_a) const
{
	return hashables.source == other_a.hashables.source && hashables.representative == other_a.hashables.representative && hashables.account == other_a.hashables.account && work == other_a.work && signature == other_a.signature;
}

raze::block_hash raze::open_block::source () const
{
	return hashables.source;
}

raze::block_hash raze::open_block::root () const
{
	return hashables.account;
}

raze::account raze::open_block::representative () const
{
	return hashables.representative;
}

raze::signature raze::open_block::block_signature () const
{
	return signature;
}

void raze::open_block::signature_set (raze::uint512_union const & signature_a)
{
	signature = signature_a;
}

raze::change_hashables::change_hashables (raze::block_hash const & previous_a, raze::account const & representative_a) :
previous (previous_a),
representative (representative_a)
{
}

raze::change_hashables::change_hashables (bool & error_a, raze::stream & stream_a)
{
	error_a = raze::read (stream_a, previous);
	if (!error_a)
	{
		error_a = raze::read (stream_a, representative);
	}
}

raze::change_hashables::change_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void raze::change_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
}

raze::change_block::change_block (raze::block_hash const & previous_a, raze::account const & representative_a, raze::raw_key const & prv_a, raze::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, representative_a),
signature (raze::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

raze::change_block::change_block (bool & error_a, raze::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = raze::read (stream_a, signature);
		if (!error_a)
		{
			error_a = raze::read (stream_a, work);
		}
	}
}

raze::change_block::change_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = raze::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void raze::change_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t raze::change_block::block_work () const
{
	return work;
}

void raze::change_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

raze::block_hash raze::change_block::previous () const
{
	return hashables.previous;
}

void raze::change_block::serialize (raze::stream & stream_a) const
{
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	write (stream_a, signature);
	write (stream_a, work);
}

void raze::change_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "change");
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("work", raze::to_string_hex (work));
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

bool raze::change_block::deserialize (raze::stream & stream_a)
{
	auto error (read (stream_a, hashables.previous));
	if (!error)
	{
		error = read (stream_a, hashables.representative);
		if (!error)
		{
			error = read (stream_a, signature);
			if (!error)
			{
				error = read (stream_a, work);
			}
		}
	}
	return error;
}

bool raze::change_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "change");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = raze::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void raze::change_block::visit (raze::block_visitor & visitor_a) const
{
	visitor_a.change_block (*this);
}

raze::block_type raze::change_block::type () const
{
	return raze::block_type::change;
}

bool raze::change_block::operator== (raze::block const & other_a) const
{
	auto other_l (dynamic_cast<raze::change_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

bool raze::change_block::operator== (raze::change_block const & other_a) const
{
	return hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && work == other_a.work && signature == other_a.signature;
}

raze::block_hash raze::change_block::source () const
{
	return 0;
}

raze::block_hash raze::change_block::root () const
{
	return hashables.previous;
}

raze::account raze::change_block::representative () const
{
	return hashables.representative;
}

raze::signature raze::change_block::block_signature () const
{
	return signature;
}

void raze::change_block::signature_set (raze::uint512_union const & signature_a)
{
	signature = signature_a;
}

std::unique_ptr<raze::block> raze::deserialize_block_json (boost::property_tree::ptree const & tree_a)
{
	std::unique_ptr<raze::block> result;
	try
	{
		auto type (tree_a.get<std::string> ("type"));
		if (type == "receive")
		{
			bool error;
			std::unique_ptr<raze::receive_block> obj (new raze::receive_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "send")
		{
			bool error;
			std::unique_ptr<raze::send_block> obj (new raze::send_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "open")
		{
			bool error;
			std::unique_ptr<raze::open_block> obj (new raze::open_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "change")
		{
			bool error;
			std::unique_ptr<raze::change_block> obj (new raze::change_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
	}
	catch (std::runtime_error const &)
	{
	}
	return result;
}

std::unique_ptr<raze::block> raze::deserialize_block (raze::stream & stream_a)
{
	raze::block_type type;
	auto error (read (stream_a, type));
	std::unique_ptr<raze::block> result;
	if (!error)
	{
		result = raze::deserialize_block (stream_a, type);
	}
	return result;
}

std::unique_ptr<raze::block> raze::deserialize_block (raze::stream & stream_a, raze::block_type type_a)
{
	std::unique_ptr<raze::block> result;
	switch (type_a)
	{
		case raze::block_type::receive:
		{
			bool error;
			std::unique_ptr<raze::receive_block> obj (new raze::receive_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case raze::block_type::send:
		{
			bool error;
			std::unique_ptr<raze::send_block> obj (new raze::send_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case raze::block_type::open:
		{
			bool error;
			std::unique_ptr<raze::open_block> obj (new raze::open_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		case raze::block_type::change:
		{
			bool error;
			std::unique_ptr<raze::change_block> obj (new raze::change_block (error, stream_a));
			if (!error)
			{
				result = std::move (obj);
			}
			break;
		}
		default:
			assert (false);
			break;
	}
	return result;
}

void raze::receive_block::visit (raze::block_visitor & visitor_a) const
{
	visitor_a.receive_block (*this);
}

bool raze::receive_block::operator== (raze::receive_block const & other_a) const
{
	auto result (hashables.previous == other_a.hashables.previous && hashables.source == other_a.hashables.source && work == other_a.work && signature == other_a.signature);
	return result;
}

bool raze::receive_block::deserialize (raze::stream & stream_a)
{
	auto error (false);
	error = read (stream_a, hashables.previous.bytes);
	if (!error)
	{
		error = read (stream_a, hashables.source.bytes);
		if (!error)
		{
			error = read (stream_a, signature.bytes);
			if (!error)
			{
				error = read (stream_a, work);
			}
		}
	}
	return error;
}

bool raze::receive_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "receive");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.source.decode_hex (source_l);
			if (!error)
			{
				error = raze::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void raze::receive_block::serialize (raze::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.source.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

void raze::receive_block::serialize_json (std::string & string_a) const
{
	boost::property_tree::ptree tree;
	tree.put ("type", "receive");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	std::string source;
	hashables.source.encode_hex (source);
	tree.put ("source", source);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", raze::to_string_hex (work));
	tree.put ("signature", signature_l);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree);
	string_a = ostream.str ();
}

raze::receive_block::receive_block (raze::block_hash const & previous_a, raze::block_hash const & source_a, raze::raw_key const & prv_a, raze::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, source_a),
signature (raze::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

raze::receive_block::receive_block (bool & error_a, raze::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		error_a = raze::read (stream_a, signature);
		if (!error_a)
		{
			error_a = raze::read (stream_a, work);
		}
	}
}

raze::receive_block::receive_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = raze::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void raze::receive_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t raze::receive_block::block_work () const
{
	return work;
}

void raze::receive_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

bool raze::receive_block::operator== (raze::block const & other_a) const
{
	auto other_l (dynamic_cast<raze::receive_block const *> (&other_a));
	auto result (other_l != nullptr);
	if (result)
	{
		result = *this == *other_l;
	}
	return result;
}

raze::block_hash raze::receive_block::previous () const
{
	return hashables.previous;
}

raze::block_hash raze::receive_block::source () const
{
	return hashables.source;
}

raze::block_hash raze::receive_block::root () const
{
	return hashables.previous;
}

raze::account raze::receive_block::representative () const
{
	return 0;
}

raze::signature raze::receive_block::block_signature () const
{
	return signature;
}

void raze::receive_block::signature_set (raze::uint512_union const & signature_a)
{
	signature = signature_a;
}

raze::block_type raze::receive_block::type () const
{
	return raze::block_type::receive;
}

raze::receive_hashables::receive_hashables (raze::block_hash const & previous_a, raze::block_hash const & source_a) :
previous (previous_a),
source (source_a)
{
}

raze::receive_hashables::receive_hashables (bool & error_a, raze::stream & stream_a)
{
	error_a = raze::read (stream_a, previous.bytes);
	if (!error_a)
	{
		error_a = raze::read (stream_a, source.bytes);
	}
}

raze::receive_hashables::receive_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = source.decode_hex (source_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void raze::receive_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
}
