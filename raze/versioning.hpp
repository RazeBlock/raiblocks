#pragma once

#include <raze/lib/blocks.hpp>
#include <raze/node/utility.hpp>

namespace raze
{
class account_info_v1
{
public:
	account_info_v1 ();
	account_info_v1 (MDB_val const &);
	account_info_v1 (raze::account_info_v1 const &) = default;
	account_info_v1 (raze::block_hash const &, raze::block_hash const &, raze::amount const &, uint64_t);
	void serialize (raze::stream &) const;
	bool deserialize (raze::stream &);
	raze::mdb_val val () const;
	raze::block_hash head;
	raze::block_hash rep_block;
	raze::amount balance;
	uint64_t modified;
};
class pending_info_v3
{
public:
	pending_info_v3 ();
	pending_info_v3 (MDB_val const &);
	pending_info_v3 (raze::account const &, raze::amount const &, raze::account const &);
	void serialize (raze::stream &) const;
	bool deserialize (raze::stream &);
	bool operator== (raze::pending_info_v3 const &) const;
	raze::mdb_val val () const;
	raze::account source;
	raze::amount amount;
	raze::account destination;
};
// Latest information about an account
class account_info_v5
{
public:
	account_info_v5 ();
	account_info_v5 (MDB_val const &);
	account_info_v5 (raze::account_info_v5 const &) = default;
	account_info_v5 (raze::block_hash const &, raze::block_hash const &, raze::block_hash const &, raze::amount const &, uint64_t);
	void serialize (raze::stream &) const;
	bool deserialize (raze::stream &);
	raze::mdb_val val () const;
	raze::block_hash head;
	raze::block_hash rep_block;
	raze::block_hash open_block;
	raze::amount balance;
	uint64_t modified;
};
}
