#include "jconf.hpp"
#include "jconf_p.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "rapidjson/error/en.h"

jconfPrivate::jconfPrivate(jconf* q) : q(q)
{
}

jconf::jconf() : d(*new jconfPrivate(this))
{
}

uint32_t jconf::get_network_timeout()
{
	return d.configValues[iNetworkTimeout]->GetUint();
}

uint32_t jconf::get_network_retry()
{
	return d.configValues[iNetworkRetry]->GetUint();
}

size_t jconf::get_pool_count()
{
	if(d.configValues[aPoolList]->IsArray())
		return d.configValues[aPoolList]->Size();
	else
		return 0;
}

jconf::pool_cfg jconf::get_pool_config(size_t n)
{
	assert(n < get_pool_count());

	jconf::pool_cfg ret;
	lpcJsVal jaddr, jlogin, jrigid, jpasswd, jtls, jtlsfp;
	const Value& oPoolConf = d.configValues[aPoolList]->GetArray()[n];

	/* We already checked presence and types */
	jaddr = GetObjectMember(oPoolConf, "pool_address");
	jlogin = GetObjectMember(oPoolConf, "wallet_address");
	jrigid = GetObjectMember(oPoolConf, "rig_id");
	jpasswd = GetObjectMember(oPoolConf, "pool_password");
	jtls = GetObjectMember(oPoolConf, "use_tls");
	jtlsfp = GetObjectMember(oPoolConf, "tls_fingerprint");

	ret.sPoolAddr = jaddr->GetString();
	ret.sWalletAddr = jlogin->GetString();
	ret.sRigId = jrigid->GetString();
	ret.sPasswd = jpasswd->GetString();
	ret.tls = jtls->GetBool();
	ret.tls_fingerprint = jtlsfp->GetString();

	return ret;
}

inline std::string read_file(const char* filename, bool& error)
{
	std::stringstream fs;

	fs << "{";
	try
	{
		std::ifstream inf(filename);
		inf.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		fs << inf.rdbuf();
	}
	catch(const std::exception& e)
	{
		error = true;
		fprintf(stderr, "Failed to read config file %s", filename);
		return "";
	}
	fs << "}";

	std::string ret = fs.str();
	if(ret.length() < 8)
	{
		error = true;
		fprintf(stderr, "Config file %s is too short", filename);
		return "";
	}

	error = false;
	std::size_t found = ret.find("\xef\xbb\xbf");

	//Replace BOM with spaces if it is there
	if(found != std::string::npos)
		ret.replace(found, 3, 3, ' ');

	return ret;
}

bool jconf::parse_config(const char* filename)
{
	bool rd_error;
	std::string file = read_file(filename, rd_error);

	if(rd_error)
		return false;

	d.jsonDoc.Parse<kParseCommentsFlag | kParseTrailingCommasFlag>(file.c_str(), file.length());

	if(d.jsonDoc.HasParseError())
	{
		fprintf(stderr, "JSON config parse error(offset %zu): %s", d.jsonDoc.GetErrorOffset(), GetParseError_En(d.jsonDoc.GetParseError()));
		return false;
	}

	if(!d.jsonDoc.IsObject())
	{
		fputs("JSON root is not an object.", stderr);
		return false;
	}

	for(size_t i = 0; i < iConfigCnt; i++)
	{
		if(oConfigValues[i].iName != i)
		{
			fputs("Code error. oConfigValues are not in order.", stderr);
			return false;
		}

		d.configValues[i] = GetObjectMember(d.jsonDoc, oConfigValues[i].sName);

		if(d.configValues[i] == nullptr)
		{
			fprintf(stderr, "Invalid config file. Missing value \"%s\".", oConfigValues[i].sName);
			return false;
		}

		if(!checkType(d.configValues[i]->GetType(), oConfigValues[i].iType))
		{
			fprintf(stderr, "Invalid config file. Value \"%s\" has unexpected type.", oConfigValues[i].sName);
			return false;
		}

		if((oConfigValues[i].flagConstraints & flag_unsigned) != 0 && !check_constraint_unsigned(d.configValues[i]))
		{
			fprintf(stderr, "Invalid config file. Value \"%s\" needs to be unsigned.", oConfigValues[i].sName);
			return false;
		}

		if((oConfigValues[i].flagConstraints & flag_u16bit) != 0 && !check_constraint_u16bit(d.configValues[i]))
		{
			fprintf(stderr, "Invalid config file. Value \"%s\" needs to be lower than 65535.", oConfigValues[i].sName);
			return false;
		}
	}

	size_t pool_cnt = d.configValues[aPoolList]->Size();
	if(pool_cnt == 0)
	{
		fprintf(stderr, "Invalid config file. pool_list must not be empty.");
		return false;
	}

	const char* aPoolValues[] = {"pool_address", "wallet_address", "rig_id", "pool_password", "use_tls", "tls_fingerprint"};
	Type poolValTypes[] = {kStringType, kStringType, kStringType, kStringType, kTrueType, kStringType };

	constexpr size_t pvcnt = sizeof(aPoolValues) / sizeof(aPoolValues[0]);
	for(uint32_t i = 0; i < pool_cnt; i++)
	{
		const Value& oThdConf = d.configValues[aPoolList]->GetArray()[i];
		
		if(!oThdConf.IsObject())
		{
			fprintf(stderr, "Invalid config file. pool_list must contain objects.");
			return false;
		}
		
		for(uint32_t j = 0; j < pvcnt; j++)
		{
			const Value* v;
			if((v = GetObjectMember(oThdConf, aPoolValues[j])) == nullptr)
			{
				fprintf(stderr, "Invalid config file. Pool %u does not have the value %s.", i, aPoolValues[j]);
				return false;
			}
			
			if(!checkType(v->GetType(), poolValTypes[j]))
			{
				fprintf(stderr, "Invalid config file. Value %s for pool %u has unexpected type.", aPoolValues[j], i);
				return false;
			}
		}
	}

	return true;
}
