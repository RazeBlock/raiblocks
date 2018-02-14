#pragma once

#include <raze/node/node.hpp>

namespace raze
{
class system
{
public:
	system (uint16_t, size_t);
	~system ();
	void generate_activity (raze::node &, std::vector<raze::account> &);
	void generate_mass_activity (uint32_t, raze::node &);
	void generate_usage_traffic (uint32_t, uint32_t, size_t);
	void generate_usage_traffic (uint32_t, uint32_t);
	raze::account get_random_account (std::vector<raze::account> &);
	raze::uint128_t get_random_amount (MDB_txn *, raze::node &, raze::account const &);
	void generate_rollback (raze::node &, std::vector<raze::account> &);
	void generate_change_known (raze::node &, std::vector<raze::account> &);
	void generate_change_unknown (raze::node &, std::vector<raze::account> &);
	void generate_receive (raze::node &);
	void generate_send_new (raze::node &, std::vector<raze::account> &);
	void generate_send_existing (raze::node &, std::vector<raze::account> &);
	std::shared_ptr<raze::wallet> wallet (size_t);
	raze::account account (MDB_txn *, size_t);
	void poll ();
	void stop ();
	boost::asio::io_service service;
	raze::alarm alarm;
	std::vector<std::shared_ptr<raze::node>> nodes;
	raze::logging logging;
	raze::work_pool work;
};
class landing_store
{
public:
	landing_store ();
	landing_store (raze::account const &, raze::account const &, uint64_t, uint64_t);
	landing_store (bool &, std::istream &);
	raze::account source;
	raze::account destination;
	uint64_t start;
	uint64_t last;
	bool deserialize (std::istream &);
	void serialize (std::ostream &) const;
	bool operator== (raze::landing_store const &) const;
};
class landing
{
public:
	landing (raze::node &, std::shared_ptr<raze::wallet>, raze::landing_store &, boost::filesystem::path const &);
	void write_store ();
	raze::uint128_t distribution_amount (uint64_t);
	void distribute_one ();
	void distribute_ongoing ();
	boost::filesystem::path path;
	raze::landing_store & store;
	std::shared_ptr<raze::wallet> wallet;
	raze::node & node;
	static int constexpr interval_exponent = 10;
	static std::chrono::seconds constexpr distribution_interval = std::chrono::seconds (1 << interval_exponent); // 1024 seconds
	static std::chrono::seconds constexpr sleep_seconds = std::chrono::seconds (7);
};
}
