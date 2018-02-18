#ifndef XRB_INTERFACE_H
#define XRB_INTERFACE_H

#if __cplusplus
extern "C" {
#endif

typedef unsigned char * raze_uint256; // 32byte array for public and private keys
typedef unsigned char * raze_uint512; // 64byte array for signatures
typedef void * raze_transaction;

// Convert public/private key bytes 'source' to a 64 byte not-null-terminated hex string 'destination'
void raze_uint256_to_string (const raze_uint256 source, char * destination);
// Convert public key bytes 'source' to a 65 byte non-null-terminated account string 'destination'
void raze_uint256_to_address (raze_uint256 source, char * destination);
// Convert public/private key bytes 'source' to a 128 byte not-null-terminated hex string 'destination'
void raze_uint512_to_string (const raze_uint512 source, char * destination);

// Convert 64 byte hex string 'source' to a byte array 'destination'
// Return 0 on success, nonzero on error
int raze_uint256_from_string (const char * source, raze_uint256 destination);
// Convert 128 byte hex string 'source' to a byte array 'destination'
// Return 0 on success, nonzero on error
int raze_uint512_from_string (const char * source, raze_uint512 destination);

// Check if the null-terminated string 'account' is a valid raze account number
// Return 0 on correct, nonzero on invalid
int raze_valid_address (const char * account);

// Create a new random number in to 'destination'
void raze_generate_random (raze_uint256 destination);
// Retrieve the detereministic private key for 'seed' at 'index'
void raze_seed_key (const raze_uint256 seed, int index, raze_uint256);
// Derive the public key 'pub' from 'key'
void raze_key_account (raze_uint256 key, raze_uint256 pub);

// Sign 'transaction' using 'private_key' and write to 'signature'
char * raze_sign_transaction (const char * transaction, const raze_uint256 private_key);
// Generate work for 'transaction'
char * raze_work_transaction (const char * transaction);

#if __cplusplus
} // extern "C"
#endif

#endif // XRB_INTERFACE_H
