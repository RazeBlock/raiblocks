#pragma once

#include <raze/node/common.hpp>
#include <raze/node/openclwork.hpp>
#include <raze/secure.hpp>

#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

namespace raze
{
// The fan spreads a key out over the heap to decrease the likelihood of it being recovered by memory inspection
class fan
{
public:
	fan (raze::uint256_union const &, size_t);
	void value (raze::raw_key &);
	void value_set (raze::raw_key const &);
	std::vector<std::unique_ptr<raze::uint256_union>> values;

private:
	std::mutex mutex;
	void value_get (raze::raw_key &);
};
class wallet_value
{
public:
	wallet_value () = default;
	wallet_value (raze::mdb_val const &);
	wallet_value (raze::uint256_union const &, uint64_t);
	raze::mdb_val val () const;
	raze::private_key key;
	uint64_t work;
};
class node_config;
class kdf
{
public:
	void phs (raze::raw_key &, std::string const &, raze::uint256_union const &);
	std::mutex mutex;
};
enum class key_type
{
	not_a_type,
	unknown,
	adhoc,
	deterministic
};
class wallet_store
{
public:
	wallet_store (bool &, raze::kdf &, raze::transaction &, raze::account, unsigned, std::string const &);
	wallet_store (bool &, raze::kdf &, raze::transaction &, raze::account, unsigned, std::string const &, std::string const &);
	std::vector<raze::account> accounts (MDB_txn *);
	void initialize (MDB_txn *, bool &, std::string const &);
	raze::uint256_union check (MDB_txn *);
	bool rekey (MDB_txn *, std::string const &);
	bool valid_password (MDB_txn *);
	bool attempt_password (MDB_txn *, std::string const &);
	void wallet_key (raze::raw_key &, MDB_txn *);
	void seed (raze::raw_key &, MDB_txn *);
	void seed_set (MDB_txn *, raze::raw_key const &);
	raze::key_type key_type (raze::wallet_value const &);
	raze::public_key deterministic_insert (MDB_txn *);
	void deterministic_key (raze::raw_key &, MDB_txn *, uint32_t);
	uint32_t deterministic_index_get (MDB_txn *);
	void deterministic_index_set (MDB_txn *, uint32_t);
	void deterministic_clear (MDB_txn *);
	raze::uint256_union salt (MDB_txn *);
	bool is_representative (MDB_txn *);
	raze::account representative (MDB_txn *);
	void representative_set (MDB_txn *, raze::account const &);
	raze::public_key insert_adhoc (MDB_txn *, raze::raw_key const &);
	void erase (MDB_txn *, raze::public_key const &);
	raze::wallet_value entry_get_raw (MDB_txn *, raze::public_key const &);
	void entry_put_raw (MDB_txn *, raze::public_key const &, raze::wallet_value const &);
	bool fetch (MDB_txn *, raze::public_key const &, raze::raw_key &);
	bool exists (MDB_txn *, raze::public_key const &);
	void destroy (MDB_txn *);
	raze::store_iterator find (MDB_txn *, raze::uint256_union const &);
	raze::store_iterator begin (MDB_txn *, raze::uint256_union const &);
	raze::store_iterator begin (MDB_txn *);
	raze::store_iterator end ();
	void derive_key (raze::raw_key &, MDB_txn *, std::string const &);
	void serialize_json (MDB_txn *, std::string &);
	void write_backup (MDB_txn *, boost::filesystem::path const &);
	bool move (MDB_txn *, raze::wallet_store &, std::vector<raze::public_key> const &);
	bool import (MDB_txn *, raze::wallet_store &);
	bool work_get (MDB_txn *, raze::public_key const &, uint64_t &);
	void work_put (MDB_txn *, raze::public_key const &, uint64_t);
	unsigned version (MDB_txn *);
	void version_put (MDB_txn *, unsigned);
	void upgrade_v1_v2 ();
	void upgrade_v2_v3 ();
	raze::fan password;
	raze::fan wallet_key_mem;
	static unsigned const version_1;
	static unsigned const version_2;
	static unsigned const version_3;
	static unsigned const version_current;
	static raze::uint256_union const version_special;
	static raze::uint256_union const wallet_key_special;
	static raze::uint256_union const salt_special;
	static raze::uint256_union const check_special;
	static raze::uint256_union const representative_special;
	static raze::uint256_union const seed_special;
	static raze::uint256_union const deterministic_index_special;
	static int const special_count;
	static unsigned const kdf_full_work = 64 * 1024;
	static unsigned const kdf_test_work = 8;
	static unsigned const kdf_work = raze::raze_network == raze::raze_networks::raze_test_network ? kdf_test_work : kdf_full_work;
	raze::kdf & kdf;
	raze::mdb_env & environment;
	MDB_dbi handle;
	std::recursive_mutex mutex;
};
class node;
// A wallet is a set of account keys encrypted by a common encryption key
class wallet : public std::enable_shared_from_this<raze::wallet>
{
public:
	std::shared_ptr<raze::block> change_action (raze::account const &, raze::account const &, bool = true);
	std::shared_ptr<raze::block> receive_action (raze::send_block const &, raze::account const &, raze::uint128_union const &, bool = true);
	std::shared_ptr<raze::block> send_action (raze::account const &, raze::account const &, raze::uint128_t const &, bool = true, boost::optional<std::string> = {});
	wallet (bool &, raze::transaction &, raze::node &, std::string const &);
	wallet (bool &, raze::transaction &, raze::node &, std::string const &, std::string const &);
	void enter_initial_password ();
	bool valid_password ();
	bool enter_password (std::string const &);
	raze::public_key insert_adhoc (raze::raw_key const &, bool = true);
	raze::public_key insert_adhoc (MDB_txn *, raze::raw_key const &, bool = true);
	raze::public_key deterministic_insert (MDB_txn *, bool = true);
	raze::public_key deterministic_insert (bool = true);
	bool exists (raze::public_key const &);
	bool import (std::string const &, std::string const &);
	void serialize (std::string &);
	bool change_sync (raze::account const &, raze::account const &);
	void change_async (raze::account const &, raze::account const &, std::function<void(std::shared_ptr<raze::block>)> const &, bool = true);
	bool receive_sync (std::shared_ptr<raze::block>, raze::account const &, raze::uint128_t const &);
	void receive_async (std::shared_ptr<raze::block>, raze::account const &, raze::uint128_t const &, std::function<void(std::shared_ptr<raze::block>)> const &, bool = true);
	raze::block_hash send_sync (raze::account const &, raze::account const &, raze::uint128_t const &);
	void send_async (raze::account const &, raze::account const &, raze::uint128_t const &, std::function<void(std::shared_ptr<raze::block>)> const &, bool = true, boost::optional<std::string> = {});
	void work_generate (raze::account const &, raze::block_hash const &);
	void work_update (MDB_txn *, raze::account const &, raze::block_hash const &, uint64_t);
	uint64_t work_fetch (MDB_txn *, raze::account const &, raze::block_hash const &);
	void work_ensure (MDB_txn *, raze::account const &);
	bool search_pending ();
	void init_free_accounts (MDB_txn *);
	std::unordered_set<raze::account> free_accounts;
	std::function<void(bool, bool)> lock_observer;
	raze::wallet_store store;
	raze::node & node;
};
// The wallets set is all the wallets a node controls.  A node may contain multiple wallets independently encrypted and operated.
class wallets
{
public:
	wallets (bool &, raze::node &);
	~wallets ();
	std::shared_ptr<raze::wallet> open (raze::uint256_union const &);
	std::shared_ptr<raze::wallet> create (raze::uint256_union const &);
	bool search_pending (raze::uint256_union const &);
	void search_pending_all ();
	void destroy (raze::uint256_union const &);
	void do_wallet_actions ();
	void queue_wallet_action (raze::uint128_t const &, std::function<void()> const &);
	void foreach_representative (MDB_txn *, std::function<void(raze::public_key const &, raze::raw_key const &)> const &);
	bool exists (MDB_txn *, raze::public_key const &);
	void stop ();
	std::function<void(bool)> observer;
	std::unordered_map<raze::uint256_union, std::shared_ptr<raze::wallet>> items;
	std::multimap<raze::uint128_t, std::function<void()>, std::greater<raze::uint128_t>> actions;
	std::mutex mutex;
	std::condition_variable condition;
	raze::kdf kdf;
	MDB_dbi handle;
	MDB_dbi send_action_ids;
	raze::node & node;
	bool stopped;
	std::thread thread;
	static raze::uint128_t const generate_priority;
	static raze::uint128_t const high_priority;
};
}
