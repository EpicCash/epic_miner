#pragma once
#include "vector32.hpp"
#include <inttypes.h>
#include <stddef.h>

class jconf
{
public:
	static jconf& inst()
	{
		static jconf inst;
		return inst;
	};

	struct pool_cfg
	{
		const char* sPoolAddr;
		const char* sWalletAddr;
		const char* sRigId;
		const char* sPasswd;
		bool tls;
		const char* tls_fingerprint;
	};

	bool parse_config(const char* filename);

	uint32_t get_network_timeout();
	uint32_t get_network_retry();

	size_t get_pool_count();
	pool_cfg get_pool_config(size_t n);

private:
	jconf();

	class jconfPrivate& d;
};
