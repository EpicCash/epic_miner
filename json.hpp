#pragma once

#include "rapidjson/document.h"

using namespace rapidjson;

typedef const Value* lpcJsVal;

/* This macro brings rapidjson more in line with other libs */
inline lpcJsVal GetObjectMember(const Value& obj, const char* key)
{
	Value::ConstMemberIterator itr = obj.FindMember(key);
	if (itr != obj.MemberEnd())
		return &itr->value;
	else
		return nullptr;
}

typedef GenericDocument<UTF8<>, MemoryPoolAllocator<>, MemoryPoolAllocator<>> MemDocument;

