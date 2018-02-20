#pragma once

#include <raze/lib/blocks.hpp>
#include <raze/node/utility.hpp>

#include <boost/property_tree/ptree.hpp>

#include <unordered_map>
#include <unordered_set>

#include <blake2/blake2.h>

namespace boost
{
template <>
struct hash<raze::uint256_union>
{
	size_t operator() (raze::uint256_union const & value_a) const
	{
		std::hash<raze::uint256_union> hash;
		return hash (value_a);
	}
};
}
namespace raze
{
class keypair
{
public:
	keypair ();
	keypair (std::string const &);
	raze::public_key pub;
	raze::raw_key prv;
};
class shared_ptr_block_hash
{
public:
	size_t operator() (std::shared_ptr<raze::block> const &) const;
	bool operator() (std::shared_ptr<raze::block> const &, std::shared_ptr<raze::block> const &) const;
};
std::unique_ptr<raze::block> deserialize_block (MDB_val const &);
// Latest information about an account
class account_info
{
public:
	account_info ();
	account_info (MDB_val const &);
	account_info (raze::account_info const &) = default;
	account_info (raze::block_hash const &, raze::block_hash const &, raze::block_hash const &, raze::amount const &, uint64_t, uint64_t);
	void serialize (raze::stream &) const;
	bool deserialize (raze::stream &);
	bool operator== (raze::account_info const &) const;
	bool operator!= (raze::account_info const &) const;
	raze::mdb_val val () const;
	raze::block_hash head;
	raze::block_hash rep_block;
	raze::block_hash open_block;
	raze::amount balance;
	/** Seconds since posix epoch */
	uint64_t modified;
	uint64_t block_count;
};
class store_entry
{
public:
	store_entry ();
	void clear ();
	store_entry * operator-> ();
	raze::mdb_val first;
	raze::mdb_val second;
};
class store_iterator
{
public:
	store_iterator (MDB_txn *, MDB_dbi);
	store_iterator (std::nullptr_t);
	store_iterator (MDB_txn *, MDB_dbi, MDB_val const &);
	store_iterator (raze::store_iterator &&);
	store_iterator (raze::store_iterator const &) = delete;
	~store_iterator ();
	raze::store_iterator & operator++ ();
	void next_dup ();
	raze::store_iterator & operator= (raze::store_iterator &&);
	raze::store_iterator & operator= (raze::store_iterator const &) = delete;
	raze::store_entry & operator-> ();
	bool operator== (raze::store_iterator const &) const;
	bool operator!= (raze::store_iterator const &) const;
	MDB_cursor * cursor;
	raze::store_entry current;
};
// Information on an uncollected send, source account, amount, target account.
class pending_info
{
public:
	pending_info ();
	pending_info (MDB_val const &);
	pending_info (raze::account const &, raze::amount const &);
	void serialize (raze::stream &) const;
	bool deserialize (raze::stream &);
	bool operator== (raze::pending_info const &) const;
	raze::mdb_val val () const;
	raze::account source;
	raze::amount amount;
};
class pending_key
{
public:
	pending_key (raze::account const &, raze::block_hash const &);
	pending_key (MDB_val const &);
	void serialize (raze::stream &) const;
	bool deserialize (raze::stream &);
	bool operator== (raze::pending_key const &) const;
	raze::mdb_val val () const;
	raze::account account;
	raze::block_hash hash;
};
class block_info
{
public:
	block_info ();
	block_info (MDB_val const &);
	block_info (raze::account const &, raze::amount const &);
	void serialize (raze::stream &) const;
	bool deserialize (raze::stream &);
	bool operator== (raze::block_info const &) const;
	raze::mdb_val val () const;
	raze::account account;
	raze::amount balance;
};
class block_counts
{
public:
	block_counts ();
	size_t sum ();
	size_t send;
	size_t receive;
	size_t open;
	size_t change;
};
class vote
{
public:
	vote () = default;
	vote (raze::vote const &);
	vote (bool &, raze::stream &);
	vote (bool &, raze::stream &, raze::block_type);
	vote (raze::account const &, raze::raw_key const &, uint64_t, std::shared_ptr<raze::block>);
	vote (MDB_val const &);
	raze::uint256_union hash () const;
	bool operator== (raze::vote const &) const;
	bool operator!= (raze::vote const &) const;
	void serialize (raze::stream &, raze::block_type);
	void serialize (raze::stream &);
	std::string to_json () const;
	// Vote round sequence number
	uint64_t sequence;
	std::shared_ptr<raze::block> block;
	// Account that's voting
	raze::account account;
	// Signature of sequence + block hash
	raze::signature signature;
};
enum class vote_code
{
	invalid, // Vote is not signed correctly
	replay, // Vote does not have the highest sequence number, it's a replay
	vote // Vote has the highest sequence number
};
class vote_result
{
public:
	raze::vote_code code;
	std::shared_ptr<raze::vote> vote;
};
class block_store
{
public:
	block_store (bool &, boost::filesystem::path const &, int lmdb_max_dbs = 128);

	MDB_dbi block_database (raze::block_type);
	void block_put_raw (MDB_txn *, MDB_dbi, raze::block_hash const &, MDB_val);
	void block_put (MDB_txn *, raze::block_hash const &, raze::block const &, raze::block_hash const & = raze::block_hash (0));
	MDB_val block_get_raw (MDB_txn *, raze::block_hash const &, raze::block_type &);
	raze::block_hash block_successor (MDB_txn *, raze::block_hash const &);
	void block_successor_clear (MDB_txn *, raze::block_hash const &);
	std::unique_ptr<raze::block> block_get (MDB_txn *, raze::block_hash const &);
	std::unique_ptr<raze::block> block_random (MDB_txn *);
	std::unique_ptr<raze::block> block_random (MDB_txn *, MDB_dbi);
	void block_del (MDB_txn *, raze::block_hash const &);
	bool block_exists (MDB_txn *, raze::block_hash const &);
	raze::block_counts block_count (MDB_txn *);
	std::unordered_multimap<raze::block_hash, raze::block_hash> block_dependencies (MDB_txn *);

	void frontier_put (MDB_txn *, raze::block_hash const &, raze::account const &);
	raze::account frontier_get (MDB_txn *, raze::block_hash const &);
	void frontier_del (MDB_txn *, raze::block_hash const &);
	size_t frontier_count (MDB_txn *);

	void account_put (MDB_txn *, raze::account const &, raze::account_info const &);
	bool account_get (MDB_txn *, raze::account const &, raze::account_info &);
	void account_del (MDB_txn *, raze::account const &);
	bool account_exists (MDB_txn *, raze::account const &);
	raze::store_iterator latest_begin (MDB_txn *, raze::account const &);
	raze::store_iterator latest_begin (MDB_txn *);
	raze::store_iterator latest_end ();

	void pending_put (MDB_txn *, raze::pending_key const &, raze::pending_info const &);
	void pending_del (MDB_txn *, raze::pending_key const &);
	bool pending_get (MDB_txn *, raze::pending_key const &, raze::pending_info &);
	bool pending_exists (MDB_txn *, raze::pending_key const &);
	raze::store_iterator pending_begin (MDB_txn *, raze::pending_key const &);
	raze::store_iterator pending_begin (MDB_txn *);
	raze::store_iterator pending_end ();

	void block_info_put (MDB_txn *, raze::block_hash const &, raze::block_info const &);
	void block_info_del (MDB_txn *, raze::block_hash const &);
	bool block_info_get (MDB_txn *, raze::block_hash const &, raze::block_info &);
	bool block_info_exists (MDB_txn *, raze::block_hash const &);
	raze::store_iterator block_info_begin (MDB_txn *, raze::block_hash const &);
	raze::store_iterator block_info_begin (MDB_txn *);
	raze::store_iterator block_info_end ();
	raze::uint128_t block_balance (MDB_txn *, raze::block_hash const &);
	static size_t const block_info_max = 32;

	raze::uint128_t representation_get (MDB_txn *, raze::account const &);
	void representation_put (MDB_txn *, raze::account const &, raze::uint128_t const &);
	void representation_add (MDB_txn *, raze::account const &, raze::uint128_t const &);
	raze::store_iterator representation_begin (MDB_txn *);
	raze::store_iterator representation_end ();

	void unchecked_clear (MDB_txn *);
	void unchecked_put (MDB_txn *, raze::block_hash const &, std::shared_ptr<raze::block> const &);
	std::vector<std::shared_ptr<raze::block>> unchecked_get (MDB_txn *, raze::block_hash const &);
	void unchecked_del (MDB_txn *, raze::block_hash const &, raze::block const &);
	raze::store_iterator unchecked_begin (MDB_txn *);
	raze::store_iterator unchecked_begin (MDB_txn *, raze::block_hash const &);
	raze::store_iterator unchecked_end ();
	size_t unchecked_count (MDB_txn *);
	std::unordered_multimap<raze::block_hash, std::shared_ptr<raze::block>> unchecked_cache;

	void unsynced_put (MDB_txn *, raze::block_hash const &);
	void unsynced_del (MDB_txn *, raze::block_hash const &);
	bool unsynced_exists (MDB_txn *, raze::block_hash const &);
	raze::store_iterator unsynced_begin (MDB_txn *, raze::block_hash const &);
	raze::store_iterator unsynced_begin (MDB_txn *);
	raze::store_iterator unsynced_end ();

	void checksum_put (MDB_txn *, uint64_t, uint8_t, raze::checksum const &);
	bool checksum_get (MDB_txn *, uint64_t, uint8_t, raze::checksum &);
	void checksum_del (MDB_txn *, uint64_t, uint8_t);

	raze::vote_result vote_validate (MDB_txn *, std::shared_ptr<raze::vote>);
	// Return latest vote for an account from store
	std::shared_ptr<raze::vote> vote_get (MDB_txn *, raze::account const &);
	// Populate vote with the next sequence number
	std::shared_ptr<raze::vote> vote_generate (MDB_txn *, raze::account const &, raze::raw_key const &, std::shared_ptr<raze::block>);
	// Return either vote or the stored vote with a higher sequence number
	std::shared_ptr<raze::vote> vote_max (MDB_txn *, std::shared_ptr<raze::vote>);
	// Return latest vote for an account considering the vote cache
	std::shared_ptr<raze::vote> vote_current (MDB_txn *, raze::account const &);
	void flush (MDB_txn *);
	raze::store_iterator vote_begin (MDB_txn *);
	raze::store_iterator vote_end ();
	std::mutex cache_mutex;
	std::unordered_map<raze::account, std::shared_ptr<raze::vote>> vote_cache;

	void version_put (MDB_txn *, int);
	int version_get (MDB_txn *);
	void do_upgrades (MDB_txn *);
	void upgrade_v1_to_v2 (MDB_txn *);
	void upgrade_v2_to_v3 (MDB_txn *);
	void upgrade_v3_to_v4 (MDB_txn *);
	void upgrade_v4_to_v5 (MDB_txn *);
	void upgrade_v5_to_v6 (MDB_txn *);
	void upgrade_v6_to_v7 (MDB_txn *);
	void upgrade_v7_to_v8 (MDB_txn *);
	void upgrade_v8_to_v9 (MDB_txn *);
	void upgrade_v9_to_v10 (MDB_txn *);

	void clear (MDB_dbi);

	raze::mdb_env environment;
	// block_hash -> account                                        // Maps head blocks to owning account
	MDB_dbi frontiers;
	// account -> block_hash, representative, balance, timestamp    // Account to head block, representative, balance, last_change
	MDB_dbi accounts;
	// block_hash -> send_block
	MDB_dbi send_blocks;
	// block_hash -> receive_block
	MDB_dbi receive_blocks;
	// block_hash -> open_block
	MDB_dbi open_blocks;
	// block_hash -> change_block
	MDB_dbi change_blocks;
	// block_hash -> sender, amount, destination                    // Pending blocks to sender account, amount, destination account
	MDB_dbi pending;
	// block_hash -> account, balance                               // Blocks info
	MDB_dbi blocks_info;
	// account -> weight                                            // Representation
	MDB_dbi representation;
	// block_hash -> block                                          // Unchecked bootstrap blocks
	MDB_dbi unchecked;
	// block_hash ->                                                // Blocks that haven't been broadcast
	MDB_dbi unsynced;
	// (uint56_t, uint8_t) -> block_hash                            // Mapping of region to checksum
	MDB_dbi checksum;
	// account -> uint64_t											// Highest vote observed for account
	MDB_dbi vote;
	// uint256_union -> ?											// Meta information about block store
	MDB_dbi meta;
};
enum class process_result
{
	progress, // Hasn't been seen before, signed correctly
	bad_signature, // Signature was bad, forged or transmission error
	old, // Already seen and was valid
	negative_spend, // Malicious attempt to spend a negative amount
	fork, // Malicious fork based on previous
	unreceivable, // Source block doesn't exist or has already been received
	gap_previous, // Block marked as previous is unknown
	gap_source, // Block marked as source is unknown
	not_receive_from_send, // Receive does not have a send source
	account_mismatch, // Account number in open block doesn't match send destination
	opened_burn_account // The impossible happened, someone found the private key associated with the public key '0'.
};
class process_return
{
public:
	raze::process_result code;
	raze::account account;
	raze::amount amount;
	raze::account pending_account;
};
enum class tally_result
{
	vote,
	changed,
	confirm
};
class votes
{
public:
	votes (std::shared_ptr<raze::block>);
	raze::tally_result vote (std::shared_ptr<raze::vote>);
	// Root block of fork
	raze::block_hash id;
	// All votes received by account
	std::unordered_map<raze::account, std::shared_ptr<raze::block>> rep_votes;
};
class ledger
{
public:
	ledger (raze::block_store &, raze::uint128_t const & = 0);
	std::pair<raze::uint128_t, std::shared_ptr<raze::block>> winner (MDB_txn *, raze::votes const & votes_a);
	// Map of weight -> associated block, ordered greatest to least
	std::map<raze::uint128_t, std::shared_ptr<raze::block>, std::greater<raze::uint128_t>> tally (MDB_txn *, raze::votes const &);
	raze::account account (MDB_txn *, raze::block_hash const &);
	raze::uint128_t amount (MDB_txn *, raze::block_hash const &);
	raze::uint128_t balance (MDB_txn *, raze::block_hash const &);
	raze::uint128_t account_balance (MDB_txn *, raze::account const &);
	raze::uint128_t account_pending (MDB_txn *, raze::account const &);
	raze::uint128_t weight (MDB_txn *, raze::account const &);
	std::unique_ptr<raze::block> successor (MDB_txn *, raze::block_hash const &);
	std::unique_ptr<raze::block> forked_block (MDB_txn *, raze::block const &);
	raze::block_hash latest (MDB_txn *, raze::account const &);
	raze::block_hash latest_root (MDB_txn *, raze::account const &);
	raze::block_hash representative (MDB_txn *, raze::block_hash const &);
	raze::block_hash representative_calculated (MDB_txn *, raze::block_hash const &);
	bool block_exists (raze::block_hash const &);
	std::string block_text (char const *);
	std::string block_text (raze::block_hash const &);
	raze::uint128_t supply (MDB_txn *);
	raze::process_return process (MDB_txn *, raze::block const &);
	void rollback (MDB_txn *, raze::block_hash const &);
	void change_latest (MDB_txn *, raze::account const &, raze::block_hash const &, raze::account const &, raze::uint128_union const &, uint64_t);
	void checksum_update (MDB_txn *, raze::block_hash const &);
	raze::checksum checksum (MDB_txn *, raze::account const &, raze::account const &);
	void dump_account_chain (raze::account const &);
	static raze::uint128_t const unit;
	raze::block_store & store;
	raze::uint128_t inactive_supply;
	std::unordered_map<raze::account, raze::uint128_t> bootstrap_weights;
	uint64_t bootstrap_weight_max_blocks;
	std::atomic<bool> check_bootstrap_weights;
};
extern raze::keypair const & zero_key;
extern raze::keypair const & test_genesis_key;
extern raze::account const & raze_test_account;
extern raze::account const & raze_beta_account;
extern raze::account const & raze_live_account;
extern std::string const & raze_test_genesis;
extern std::string const & raze_beta_genesis;
extern std::string const & raze_live_genesis;
extern std::string const & genesis_block;
extern raze::account const & genesis_account;
extern raze::account const & burn_account;
extern raze::uint128_t const & genesis_amount;
// A block hash that compares inequal to any real block hash
extern raze::block_hash const & not_a_block;
// An account number that compares inequal to any real account number
extern raze::block_hash const & not_an_account;
class genesis
{
public:
	explicit genesis ();
	void initialize (MDB_txn *, raze::block_store &) const;
	raze::block_hash hash () const;
	std::unique_ptr<raze::open_block> open;
};
}
