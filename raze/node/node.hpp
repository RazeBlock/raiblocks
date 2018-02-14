#pragma once

#include <raze/lib/work.hpp>
#include <raze/node/bootstrap.hpp>
#include <raze/node/wallet.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/log/trivial.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <miniupnpc.h>

namespace boost
{
namespace program_options
{
	class options_description;
	class variables_map;
}
}

namespace raze
{
class node;
class election : public std::enable_shared_from_this<raze::election>
{
	std::function<void(std::shared_ptr<raze::block>, bool)> confirmation_action;
	void confirm_once (MDB_txn *);

public:
	election (MDB_txn *, raze::node &, std::shared_ptr<raze::block>, std::function<void(std::shared_ptr<raze::block>, bool)> const &);
	void vote (std::shared_ptr<raze::vote>);
	// Check if we have vote quorum
	bool have_quorum (MDB_txn *);
	// Tell the network our view of the winner
	void broadcast_winner ();
	// Change our winner to agree with the network
	void compute_rep_votes (MDB_txn *);
	// Confirmation method 1, uncontested quorum
	void confirm_if_quorum (MDB_txn *);
	// Confirmation method 2, settling time
	void confirm_cutoff (MDB_txn *);
	raze::uint128_t quorum_threshold (MDB_txn *, raze::ledger &);
	raze::uint128_t minimum_threshold (MDB_txn *, raze::ledger &);
	raze::votes votes;
	raze::node & node;
	std::chrono::steady_clock::time_point last_vote;
	std::shared_ptr<raze::block> last_winner;
	std::atomic_flag confirmed;
};
class conflict_info
{
public:
	raze::block_hash root;
	std::shared_ptr<raze::election> election;
	// Number of announcements in a row for this fork
	unsigned announcements;
};
// Core class for determining concensus
// Holds all active blocks i.e. recently added blocks that need confirmation
class active_transactions
{
public:
	active_transactions (raze::node &);
	// Start an election for a block
	// Call action with confirmed block, may be different than what we started with
	bool start (MDB_txn *, std::shared_ptr<raze::block>, std::function<void(std::shared_ptr<raze::block>, bool)> const & = [](std::shared_ptr<raze::block>, bool) {});
	void vote (std::shared_ptr<raze::vote>);
	// Is the root of this block in the roots container
	bool active (raze::block const &);
	void announce_votes ();
	void stop ();
	boost::multi_index_container<
	raze::conflict_info,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_unique<boost::multi_index::member<raze::conflict_info, raze::block_hash, &raze::conflict_info::root>>>>
	roots;
	raze::node & node;
	std::mutex mutex;
	// Maximum number of conflicts to vote on per interval, lowest root hash first
	static unsigned constexpr announcements_per_interval = 32;
	// After this many successive vote announcements, block is confirmed
	static unsigned constexpr contigious_announcements = 4;
	static unsigned constexpr announce_interval_ms = (raze::raze_network == raze::raze_networks::raze_test_network) ? 10 : 16000;
};
class operation
{
public:
	bool operator> (raze::operation const &) const;
	std::chrono::steady_clock::time_point wakeup;
	std::function<void()> function;
};
class alarm
{
public:
	alarm (boost::asio::io_service &);
	~alarm ();
	void add (std::chrono::steady_clock::time_point const &, std::function<void()> const &);
	void run ();
	boost::asio::io_service & service;
	std::mutex mutex;
	std::condition_variable condition;
	std::priority_queue<operation, std::vector<operation>, std::greater<operation>> operations;
	std::thread thread;
};
class gap_information
{
public:
	std::chrono::steady_clock::time_point arrival;
	raze::block_hash hash;
	std::unique_ptr<raze::votes> votes;
};
class gap_cache
{
public:
	gap_cache (raze::node &);
	void add (MDB_txn *, std::shared_ptr<raze::block>);
	void vote (std::shared_ptr<raze::vote>);
	raze::uint128_t bootstrap_threshold (MDB_txn *);
	void purge_old ();
	boost::multi_index_container<
	raze::gap_information,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<gap_information, std::chrono::steady_clock::time_point, &gap_information::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<gap_information, raze::block_hash, &gap_information::hash>>>>
	blocks;
	size_t const max = 256;
	std::mutex mutex;
	raze::node & node;
};
class work_pool;
class peer_information
{
public:
	peer_information (raze::endpoint const &, unsigned);
	peer_information (raze::endpoint const &, std::chrono::steady_clock::time_point const &, std::chrono::steady_clock::time_point const &);
	raze::endpoint endpoint;
	std::chrono::steady_clock::time_point last_contact;
	std::chrono::steady_clock::time_point last_attempt;
	std::chrono::steady_clock::time_point last_bootstrap_attempt;
	std::chrono::steady_clock::time_point last_rep_request;
	std::chrono::steady_clock::time_point last_rep_response;
	raze::amount rep_weight;
	unsigned network_version;
};
class peer_attempt
{
public:
	raze::endpoint endpoint;
	std::chrono::steady_clock::time_point last_attempt;
};
class peer_container
{
public:
	peer_container (raze::endpoint const &);
	// We were contacted by endpoint, update peers
	void contacted (raze::endpoint const &, unsigned);
	// Unassigned, reserved, self
	bool not_a_peer (raze::endpoint const &);
	// Returns true if peer was already known
	bool known_peer (raze::endpoint const &);
	// Notify of peer we received from
	bool insert (raze::endpoint const &, unsigned);
	std::unordered_set<raze::endpoint> random_set (size_t);
	void random_fill (std::array<raze::endpoint, 8> &);
	// Request a list of the top known representatives
	std::vector<peer_information> representatives (size_t);
	// List of all peers
	std::vector<raze::endpoint> list ();
	std::map<raze::endpoint, unsigned> list_version ();
	// A list of random peers with size the square root of total peer count
	std::vector<raze::endpoint> list_sqrt ();
	// Get the next peer for attempting bootstrap
	raze::endpoint bootstrap_peer ();
	// Purge any peer where last_contact < time_point and return what was left
	std::vector<raze::peer_information> purge_list (std::chrono::steady_clock::time_point const &);
	std::vector<raze::endpoint> rep_crawl ();
	bool rep_response (raze::endpoint const &, raze::amount const &);
	void rep_request (raze::endpoint const &);
	// Should we reach out to this endpoint with a keepalive message
	bool reachout (raze::endpoint const &);
	size_t size ();
	size_t size_sqrt ();
	bool empty ();
	bool hash2_aware (raze::endpoint const &);
	std::mutex mutex;
	raze::endpoint self;
	boost::multi_index_container<
	peer_information,
	boost::multi_index::indexed_by<
	boost::multi_index::hashed_unique<boost::multi_index::member<peer_information, raze::endpoint, &peer_information::endpoint>>,
	boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_contact>>,
	boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_attempt>, std::greater<std::chrono::steady_clock::time_point>>,
	boost::multi_index::random_access<>,
	boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_bootstrap_attempt>>,
	boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_rep_request>>,
	boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, raze::amount, &peer_information::rep_weight>, std::greater<raze::amount>>>>
	peers;
	boost::multi_index_container<
	peer_attempt,
	boost::multi_index::indexed_by<
	boost::multi_index::hashed_unique<boost::multi_index::member<peer_attempt, raze::endpoint, &peer_attempt::endpoint>>,
	boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_attempt, std::chrono::steady_clock::time_point, &peer_attempt::last_attempt>>>>
	attempts;
	// Called when a new peer is observed
	std::function<void(raze::endpoint const &)> peer_observer;
	std::function<void()> disconnect_observer;
	// Number of peers to crawl for being a rep every period
	static size_t constexpr peers_per_crawl = 8;
};
class send_info
{
public:
	uint8_t const * data;
	size_t size;
	raze::endpoint endpoint;
	std::function<void(boost::system::error_code const &, size_t)> callback;
};
class mapping_protocol
{
public:
	char const * name;
	int remaining;
	boost::asio::ip::address_v4 external_address;
	uint16_t external_port;
};
// These APIs aren't easy to understand so comments are verbose
class port_mapping
{
public:
	port_mapping (raze::node &);
	void start ();
	void stop ();
	void refresh_devices ();
	// Refresh when the lease ends
	void refresh_mapping ();
	// Refresh ocassionally in case router loses mapping
	void check_mapping_loop ();
	int check_mapping ();
	bool has_address ();
	std::mutex mutex;
	raze::node & node;
	UPNPDev * devices; // List of all UPnP devices
	UPNPUrls urls; // Something for UPnP
	IGDdatas data; // Some other UPnP thing
	// Primes so they infrequently happen at the same time
	static int constexpr mapping_timeout = raze::raze_network == raze::raze_networks::raze_test_network ? 53 : 3593;
	static int constexpr check_timeout = raze::raze_network == raze::raze_networks::raze_test_network ? 17 : 53;
	boost::asio::ip::address_v4 address;
	std::array<mapping_protocol, 2> protocols;
	uint64_t check_count;
	bool on;
};
class message_statistics
{
public:
	message_statistics ();
	std::atomic<uint64_t> keepalive;
	std::atomic<uint64_t> publish;
	std::atomic<uint64_t> confirm_req;
	std::atomic<uint64_t> confirm_ack;
};
class block_arrival_info
{
public:
	std::chrono::steady_clock::time_point arrival;
	raze::block_hash hash;
};
// This class tracks blocks that are probably live because they arrived in a UDP packet
// This gives a fairly reliable way to differentiate between blocks being inserted via bootstrap or new, live blocks.
class block_arrival
{
public:
	void add (raze::block_hash const &);
	bool recent (raze::block_hash const &);
	boost::multi_index_container<
	raze::block_arrival_info,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<raze::block_arrival_info, std::chrono::steady_clock::time_point, &raze::block_arrival_info::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<raze::block_arrival_info, raze::block_hash, &raze::block_arrival_info::hash>>>>
	arrival;
	std::mutex mutex;
};
class network
{
public:
	network (raze::node &, uint16_t);
	void receive ();
	void stop ();
	void receive_action (boost::system::error_code const &, size_t);
	void rpc_action (boost::system::error_code const &, size_t);
	void rebroadcast_reps (std::shared_ptr<raze::block>);
	void republish_vote (std::chrono::steady_clock::time_point const &, std::shared_ptr<raze::vote>);
	void republish_block (MDB_txn *, std::shared_ptr<raze::block>);
	void republish (raze::block_hash const &, std::shared_ptr<std::vector<uint8_t>>, raze::endpoint);
	void publish_broadcast (std::vector<raze::peer_information> &, std::unique_ptr<raze::block>);
	void confirm_send (raze::confirm_ack const &, std::shared_ptr<std::vector<uint8_t>>, raze::endpoint const &);
	void merge_peers (std::array<raze::endpoint, 8> const &);
	void send_keepalive (raze::endpoint const &);
	void broadcast_confirm_req (std::shared_ptr<raze::block>);
	void send_confirm_req (raze::endpoint const &, std::shared_ptr<raze::block>);
	void send_buffer (uint8_t const *, size_t, raze::endpoint const &, std::function<void(boost::system::error_code const &, size_t)>);
	raze::endpoint endpoint ();
	raze::endpoint remote;
	std::array<uint8_t, 512> buffer;
	boost::asio::ip::udp::socket socket;
	std::mutex socket_mutex;
	boost::asio::ip::udp::resolver resolver;
	raze::node & node;
	uint64_t bad_sender_count;
	bool on;
	uint64_t insufficient_work_count;
	uint64_t error_count;
	raze::message_statistics incoming;
	raze::message_statistics outgoing;
	static uint16_t const node_port = raze::raze_network == raze::raze_networks::raze_live_network ? 7075 : 54000;
};
class logging
{
public:
	logging ();
	void serialize_json (boost::property_tree::ptree &) const;
	bool deserialize_json (bool &, boost::property_tree::ptree &);
	bool upgrade_json (unsigned, boost::property_tree::ptree &);
	bool ledger_logging () const;
	bool ledger_duplicate_logging () const;
	bool vote_logging () const;
	bool network_logging () const;
	bool network_message_logging () const;
	bool network_publish_logging () const;
	bool network_packet_logging () const;
	bool network_keepalive_logging () const;
	bool node_lifetime_tracing () const;
	bool insufficient_work_logging () const;
	bool log_rpc () const;
	bool bulk_pull_logging () const;
	bool callback_logging () const;
	bool work_generation_time () const;
	bool log_to_cerr () const;
	void init (boost::filesystem::path const &);

	bool ledger_logging_value;
	bool ledger_duplicate_logging_value;
	bool vote_logging_value;
	bool network_logging_value;
	bool network_message_logging_value;
	bool network_publish_logging_value;
	bool network_packet_logging_value;
	bool network_keepalive_logging_value;
	bool node_lifetime_tracing_value;
	bool insufficient_work_logging_value;
	bool log_rpc_value;
	bool bulk_pull_logging_value;
	bool work_generation_time_value;
	bool log_to_cerr_value;
	bool flush;
	uintmax_t max_size;
	uintmax_t rotation_size;
	boost::log::sources::logger_mt log;
};
class node_init
{
public:
	node_init ();
	bool error ();
	bool block_store_init;
	bool wallet_init;
};
class node_config
{
public:
	node_config ();
	node_config (uint16_t, raze::logging const &);
	void serialize_json (boost::property_tree::ptree &) const;
	bool deserialize_json (bool &, boost::property_tree::ptree &);
	bool upgrade_json (unsigned, boost::property_tree::ptree &);
	raze::account random_representative ();
	uint16_t peering_port;
	raze::logging logging;
	std::vector<std::pair<boost::asio::ip::address, uint16_t>> work_peers;
	std::vector<std::string> preconfigured_peers;
	std::vector<raze::account> preconfigured_representatives;
	unsigned bootstrap_fraction_numerator;
	raze::amount receive_minimum;
	raze::amount inactive_supply;
	unsigned password_fanout;
	unsigned io_threads;
	unsigned work_threads;
	bool enable_voting;
	unsigned bootstrap_connections;
	unsigned bootstrap_connections_max;
	std::string callback_address;
	uint16_t callback_port;
	std::string callback_target;
	int lmdb_max_dbs;
	static std::chrono::seconds constexpr keepalive_period = std::chrono::seconds (60);
	static std::chrono::seconds constexpr keepalive_cutoff = keepalive_period * 5;
	static std::chrono::minutes constexpr wallet_backup_interval = std::chrono::minutes (5);
};
class node_observers
{
public:
	raze::observer_set<std::shared_ptr<raze::block>, raze::account const &, raze::amount const &> blocks;
	raze::observer_set<bool> wallet;
	raze::observer_set<std::shared_ptr<raze::vote>, raze::vote_code, raze::endpoint const &> vote;
	raze::observer_set<raze::account const &, bool> account_balance;
	raze::observer_set<raze::endpoint const &> endpoint;
	raze::observer_set<> disconnect;
	raze::observer_set<> started;
};
class vote_processor
{
public:
	vote_processor (raze::node &);
	raze::vote_result vote (std::shared_ptr<raze::vote>, raze::endpoint);
	raze::node & node;
};
// The network is crawled for representatives by ocassionally sending a unicast confirm_req for a specific block and watching to see if it's acknowledged with a vote.
class rep_crawler
{
public:
	void add (raze::block_hash const &);
	void remove (raze::block_hash const &);
	bool exists (raze::block_hash const &);
	std::mutex mutex;
	std::unordered_set<raze::block_hash> active;
};
class block_processor_item
{
public:
	block_processor_item (std::shared_ptr<raze::block>);
	block_processor_item (std::shared_ptr<raze::block>, bool);
	std::shared_ptr<raze::block> block;
	bool force;
};
// Processing blocks is a potentially long IO operation
// This class isolates block insertion from other operations like servicing network operations
class block_processor
{
public:
	block_processor (raze::node &);
	~block_processor ();
	void stop ();
	void flush ();
	void add (raze::block_processor_item const &);
	void process_receive_many (raze::block_processor_item const &);
	void process_receive_many (std::deque<raze::block_processor_item> &);
	raze::process_return process_receive_one (MDB_txn *, std::shared_ptr<raze::block>);
	void process_blocks ();

private:
	bool stopped;
	bool idle;
	std::deque<raze::block_processor_item> blocks;
	std::mutex mutex;
	std::condition_variable condition;
	raze::node & node;
};
class node : public std::enable_shared_from_this<raze::node>
{
public:
	node (raze::node_init &, boost::asio::io_service &, uint16_t, boost::filesystem::path const &, raze::alarm &, raze::logging const &, raze::work_pool &);
	node (raze::node_init &, boost::asio::io_service &, boost::filesystem::path const &, raze::alarm &, raze::node_config const &, raze::work_pool &);
	~node ();
	template <typename T>
	void background (T action_a)
	{
		alarm.service.post (action_a);
	}
	void send_keepalive (raze::endpoint const &);
	bool copy_with_compaction (boost::filesystem::path const &);
	void keepalive (std::string const &, uint16_t);
	void start ();
	void stop ();
	std::shared_ptr<raze::node> shared ();
	int store_version ();
	void process_confirmed (std::shared_ptr<raze::block>);
	void process_message (raze::message &, raze::endpoint const &);
	void process_active (std::shared_ptr<raze::block>);
	raze::process_return process (raze::block const &);
	void keepalive_preconfigured (std::vector<std::string> const &);
	raze::block_hash latest (raze::account const &);
	raze::uint128_t balance (raze::account const &);
	std::unique_ptr<raze::block> block (raze::block_hash const &);
	std::pair<raze::uint128_t, raze::uint128_t> balance_pending (raze::account const &);
	raze::uint128_t weight (raze::account const &);
	raze::account representative (raze::account const &);
	void store_update ();
	void ongoing_keepalive ();
	void ongoing_rep_crawl ();
	void ongoing_bootstrap ();
	void ongoing_store_flush ();
	void v10_v11_store_update ();
	void backup_wallet ();
	int price (raze::uint128_t const &, int);
	void generate_work (raze::block &);
	uint64_t generate_work (raze::uint256_union const &);
	void generate_work (raze::uint256_union const &, std::function<void(uint64_t)>);
	void add_initial_peers ();
	boost::asio::io_service & service;
	raze::node_config config;
	raze::alarm & alarm;
	raze::work_pool & work;
	boost::log::sources::logger_mt log;
	raze::block_store store;
	raze::gap_cache gap_cache;
	raze::ledger ledger;
	raze::active_transactions active;
	raze::network network;
	raze::bootstrap_initiator bootstrap_initiator;
	raze::bootstrap_listener bootstrap;
	raze::peer_container peers;
	boost::filesystem::path application_path;
	raze::node_observers observers;
	raze::wallets wallets;
	raze::port_mapping port_mapping;
	raze::vote_processor vote_processor;
	raze::rep_crawler rep_crawler;
	unsigned warmed_up;
	raze::block_processor block_processor;
	std::thread block_processor_thread;
	raze::block_arrival block_arrival;
	static double constexpr price_max = 16.0;
	static double constexpr free_cutoff = 1024.0;
	static std::chrono::seconds constexpr period = std::chrono::seconds (60);
	static std::chrono::seconds constexpr cutoff = period * 5;
	static std::chrono::minutes constexpr backup_interval = std::chrono::minutes (5);
};
class thread_runner
{
public:
	thread_runner (boost::asio::io_service &, unsigned);
	~thread_runner ();
	void join ();
	std::vector<std::thread> threads;
};
void add_node_options (boost::program_options::options_description &);
bool handle_node_options (boost::program_options::variables_map &);
class inactive_node
{
public:
	inactive_node (boost::filesystem::path const & path = raze::working_path ());
	~inactive_node ();
	boost::filesystem::path path;
	boost::shared_ptr<boost::asio::io_service> service;
	raze::alarm alarm;
	raze::logging logging;
	raze::node_init init;
	raze::work_pool work;
	std::shared_ptr<raze::node> node;
};
}
