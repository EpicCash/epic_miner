#pragma once

#include "json.hpp"

/*
 * This enum needs to match index in oConfigValues, otherwise we will get a runtime error
 */

enum configEnum
{
	aPoolList,
	iNetworkTimeout,
	iNetworkRetry
};

struct configVal
{
	configEnum iName;
	const char* sName;
	Type iType;
	size_t flagConstraints;
};

constexpr size_t flag_none = 0;
constexpr size_t flag_unsigned = 1 << 0;
constexpr size_t flag_u16bit = 1 << 1;

inline bool check_constraint_unsigned(lpcJsVal val)
{
	return val->IsUint();
}

inline bool check_constraint_u16bit(lpcJsVal val)
{
	return val->IsUint() && val->GetUint() <= 0xffff;
}

// Same order as in configEnum, as per comment above
// kNullType means any type
configVal oConfigValues[] = {
	{aPoolList, "pool_list", kArrayType, flag_none},
	{iNetworkTimeout, "network_timeout", kNumberType, flag_unsigned},
	{iNetworkRetry, "network_retry", kNumberType, flag_unsigned}
};

constexpr size_t iConfigCnt = (sizeof(oConfigValues) / sizeof(oConfigValues[0]));

inline bool checkType(Type have, Type want)
{
	if(want == have)
		return true;
	else if(want == kNullType)
		return true;
	else if(want == kTrueType && have == kFalseType)
		return true;
	else if(want == kFalseType && have == kTrueType)
		return true;
	else
		return false;
}

class jconfPrivate
{
  private:
	friend class jconf;
	jconfPrivate(jconf* q);

	Document jsonDoc;
	lpcJsVal configValues[iConfigCnt];

	class jconf* const q;
};
