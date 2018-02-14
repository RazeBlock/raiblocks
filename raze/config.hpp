#pragma once

#include <chrono>
#include <cstddef>

namespace raze
{
// Network variants with different genesis blocks and network parameters
enum class raze_networks
{
	// Low work parameters, publicly known genesis key, test IP ports
	raze_test_network,
	// Normal work parameters, secret beta genesis key, beta IP ports
	raze_beta_network,
	// Normal work parameters, secret live key, live IP ports
	raze_live_network
};
raze::raze_networks const raze_network = raze_networks::ACTIVE_NETWORK;
std::chrono::milliseconds const transaction_timeout = std::chrono::milliseconds (1000);
}
