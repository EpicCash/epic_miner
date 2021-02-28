#include "pool.hpp"
#include "executor.hpp"
#include "console/console.hpp"
#include "misc.hpp"

pool::pool(uint32_t pool_id) : pool_id(pool_id), domAlloc(json_dom_buf, json_buffer_len),
	parseAlloc(json_parse_buf, json_buffer_len),
	jsonDoc(&domAlloc, json_buffer_len, &parseAlloc)
{
}

void pool::net_on_connect(const char* tls_fp)
{
	printf("net_on_connect\n");
	do_login_call();
}

void pool::do_login_call()
{
	uint32_t call_id;
	register_call(call_types::login, call_id);

	constexpr const char* format_str = "{\"method\":\"login\",\"params\":"
		"{\"login\":\"%s\",\"pass\":\"%s\",\"rigid\":\"%s\",\"agent\":\"%s\"},\"id\":%u}\n";
	if(tls)
	{
		char cmd_buffer[1024];
		uint32_t len = snprintf(cmd_buffer, sizeof(cmd_buffer), format_str,
			 username.c_str(), password.c_str(), rigid.c_str(), "epic_miner", call_id);
		net->do_socket_write(cmd_buffer, len);
	}
	else
	{
		uint32_t idx;
		uint32_t buflen;
		char* buffer = net->get_send_buffer(buflen, idx);
		uint32_t len = snprintf(buffer, buflen, format_str, 
			username.c_str(), password.c_str(), rigid.c_str(), "epic_miner", call_id);
		net->finalize_socket_write(idx, len);
	}
}

void pool::do_send_result(const result& res)
{
	uint32_t call_id;
	register_call(call_types::result, call_id);

	char sNonce[9];
	char sResult[65];
	bin2hex((unsigned char*)&res.nonce, 4, sNonce);
	bin2hex(res.res_hash.data, 32, sResult);

	constexpr const char* format_str = "{\"method\":\"submit\",\"params\":"
		"{\"id\":\"%s\",\"job_id\":\"%s\",\"nonce\":\"%s\",\"result\":\"%s\"},\"id\":%u}\n";

	if(tls)
	{
		char cmd_buffer[1024];
		uint32_t len = snprintf(cmd_buffer, sizeof(cmd_buffer), format_str,
								pool_miner_id.c_str(), res.id.data, sNonce, sResult, call_id);
		net->do_socket_write(cmd_buffer, len);
	}
	else
	{
		uint32_t idx;
		uint32_t buflen;
		char* buffer = net->get_send_buffer(buflen, idx);
		uint32_t len = snprintf(buffer, buflen, format_str, 
								pool_miner_id.c_str(), res.id.data, sNonce, sResult, call_id);
		printf("SEND: %s", buffer);
		net->finalize_socket_write(idx, len);
	}
}

// It is in glibc, but not on msvc
inline char* my_memrchr(char* m, char c, size_t n)
{
	while (n--) if (m[n]==c) return (char *)(m+n);
	return 0;
}

ssize_t pool::net_on_data_recv(char* data, size_t len)
{
	char* end = my_memrchr(data, '\n', len);

	if(end == nullptr)
		return 0;

	*end = '\0';
	len = static_cast<size_t>(end-data)+1;

	jsonDoc.SetNull();
	parseAlloc.Clear();
	domAlloc.Clear();
	jsonDoc.SetNull();

	printf("RECV: %s\n", data);
	if(jsonDoc.ParseInsitu(data).HasParseError() || !jsonDoc.IsObject())
	{
		net_on_error("JSON parse error");
		return -1;
	}

	if(process_json_doc())
		return len;
	else
		return -1;
}

bool pool::process_json_doc()
{
	lpcJsVal mt = GetObjectMember(jsonDoc, "method");
	if(mt != nullptr)
	{
		if(!mt->IsString())
			return protocol_error("PARSE error: Protocol error 1");
		
		if(strcmp(mt->GetString(), "job") != 0)
			return protocol_error("PARSE error: Unsupported server method.");
		
		lpcJsVal params = GetObjectMember(jsonDoc, "params");
		if(params == nullptr || !params->IsObject())
			return protocol_error("PARSE error: Protocol error 2");
		
		return process_pool_job(*params);
	}
	else
	{
		uint32_t call_id;
		mt = GetObjectMember(jsonDoc, "id");
		if(mt == nullptr || !mt->IsUint())
			return protocol_error("PARSE error: Protocol error 3");

		call_id = mt->GetUint();
		mt = GetObjectMember(jsonDoc, "error");

		const char* error_msg = nullptr;
		if(mt == nullptr || mt->IsNull())
		{
			/* If there was no error we need a result */
			if((mt = GetObjectMember(jsonDoc, "result")) == nullptr)
				return protocol_error("PARSE error: Protocol error 7");
			if(!mt->IsObject())
				return protocol_error("PARSE error: Protocol error 8");
		}
		else
		{
			if(!mt->IsObject())
				return protocol_error("PARSE error: Protocol error 5");
	
			const Value* msg = GetObjectMember(*mt, "message");

			if(msg == nullptr || !msg->IsString())
				return protocol_error("PARSE error: Protocol error 6");

			error_msg = msg->GetString();
			mt = nullptr;
		}

		switch(find_call_type(call_id))
		{
		case call_types::invalid:
			return protocol_error("Invalid call_id");
		case call_types::login:
			return process_pool_login(mt, error_msg);
		case call_types::result:
			return process_pool_result(mt, error_msg);;
		case call_types::keepalive:
			return process_pool_keepalive(mt, error_msg);;
		}
	}
}

bool pool::process_pool_job(const Value& param)
{
	lpcJsVal blob, jobid, target, motd, seed_hash;
	jobid = GetObjectMember(param, "job_id");
	blob = GetObjectMember(param, "blob");
	target = GetObjectMember(param, "target");
	motd = GetObjectMember(param, "motd");
	seed_hash = GetObjectMember(param, "seed_hash");
	
	if(jobid == nullptr || blob == nullptr || target == nullptr ||
		!jobid->IsString() || !blob->IsString() || !target->IsString())
	{
		return protocol_error("PARSE error: Job error 1");
	}

	my_job.reset();

	size_t jobid_len = jobid->GetStringLength();
	if(jobid_len >= sizeof(jobid::data)) // Note >=
		return protocol_error("PARSE error: Job error 3");
	memcpy(my_job.id.data, jobid->GetString(), jobid_len+1);

	uint32_t work_len = blob->GetStringLength() / 2;
	
	if(work_len > sizeof(pool_job::blob))
		return protocol_error("PARSE error: Invalid job length.");
	
	if(!hex2bin(blob->GetString(), work_len * 2, my_job.blob))
		return protocol_error("PARSE error: Job error 4");

	size_t target_slen = target->GetStringLength();
	if(target_slen > 8)
		return protocol_error("PARSE error: Invalid target len");

	char sTempStr[] = "00000000"; // Little-endian CPU FTW
	memcpy(sTempStr, target->GetString(), target_slen);
	if(!hex2bin(sTempStr, 8, (unsigned char*)&my_job.target) || my_job.target == 0)
		return protocol_error("PARSE error: Invalid target");

	if(seed_hash != nullptr && seed_hash->IsString() && seed_hash->GetStringLength() == 64u)
	{
		if(!hex2bin(seed_hash->GetString(), seed_hash->GetStringLength(), my_job.randomx_seed.data))
			return protocol_error("PARSE error: Invalid seed_hash");
	}

	my_job.type = pow_type::randomx;
	my_job.nonce_pos = work_len - 4;
	my_job.blob_len = work_len;

	executor::inst().on_pool_new_job(pool_id);
	return true;
}

bool pool::process_pool_login(lpcJsVal value, const char* err_msg)
{
	if(err_msg != nullptr)
	{
		return protocol_error("err_msg");
	}

	lpcJsVal id = GetObjectMember(*value, "id");
	lpcJsVal job = GetObjectMember(*value, "job");

	if(id == nullptr || job == nullptr || !id->IsString())
	{
		return protocol_error("PARSE error: Login protocol error 1");
	}

	pool_miner_id = id->GetString();
	return process_pool_job(*job);
}

bool pool::process_pool_result(lpcJsVal value, const char* err_msg)
{
	if(err_msg != nullptr)
	{
		return protocol_error("err_msg");
	}

	printf("process_pool_result - ok!\n");
	return true;
}

bool pool::process_pool_keepalive(lpcJsVal, const char* err_msg)
{
	if(err_msg != nullptr)
	{
		return protocol_error("err_msg");
	}

	return true;
}

void pool::net_on_error(const char* error)
{
}

void pool::net_on_close()
{
}
