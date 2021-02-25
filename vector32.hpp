#pragma once

#include <functional>
#include <stdint.h>
#pragma GCC target("sse4.1")
#include <x86intrin.h>

struct v32
{
	v32() {}

	v32(const uint8_t* src) 
	{ 
		__m128i r1 = _mm_loadu_si128((const __m128i*)src);
		__m128i r2 = _mm_loadu_si128((const __m128i*)(src + 16));
		_mm_store_si128((__m128i*)data, r1);
		_mm_store_si128((__m128i*)(data + 16), r2);
	}

	v32(const v32& o)
	{
		__m128i r1 = _mm_load_si128((const __m128i*)o.data);
		__m128i r2 = _mm_load_si128((const __m128i*)(o.data + 16));
		_mm_store_si128((__m128i*)data, r1);
		_mm_store_si128((__m128i*)(data + 16), r2);
	}

	uint32_t get_work32() const
	{
		return *reinterpret_cast<const uint32_t*>(data);
	}

	uint64_t get_work64() const
	{
		return *reinterpret_cast<const uint64_t*>(data);
	}

	uint64_t get_id() const
	{
		return reinterpret_cast<const uint64_t*>(data)[0];
	}

	v32& operator=(const v32& o)
	{
		__m128i r1 = _mm_load_si128((const __m128i*)o.data);
		__m128i r2 = _mm_load_si128((const __m128i*)(o.data + 16));
		_mm_store_si128((__m128i*)data, r1);
		_mm_store_si128((__m128i*)(data + 16), r2);
		return *this;
	}

	v32& operator=(const uint8_t* src)
	{
		__m128i r1 = _mm_loadu_si128((const __m128i*)src);
		__m128i r2 = _mm_loadu_si128((const __m128i*)(src + 16));
		_mm_store_si128((__m128i*)data, r1);
		_mm_store_si128((__m128i*)(data + 16), r2);
		return *this;
	}

	bool operator==(const v32& o) const
	{
		__m128i r;
		r = _mm_xor_si128(_mm_load_si128((const __m128i*)data),
						  _mm_load_si128((const __m128i*)o.data));

		if(_mm_testz_si128(r, r) == 0)
			return false;

		r = _mm_xor_si128(_mm_load_si128((const __m128i*)(data + 16)),
						  _mm_load_si128((const __m128i*)(o.data + 16)));

		return _mm_testz_si128(r, r) == 1;
	}

	void set_all_ones()
	{
		__m128i v = _mm_set1_epi64x(-1);
		_mm_store_si128((__m128i*)data, v);
		_mm_store_si128((__m128i*)(data + 16), v);
	}

	void set_all_zero()
	{
		__m128i v = _mm_setzero_si128();
		_mm_store_si128((__m128i*)data, v);
		_mm_store_si128((__m128i*)(data + 16), v);
	}

	bool operator!=(const v32& o) const { return !(*this == o); }

	operator const uint8_t*() const { return data; }
	operator uint8_t*() { return data; }

	static constexpr size_t size = 32;
	alignas(16) uint8_t data[32];
};

namespace std
{
	template<> struct hash<v32>
	{
		std::size_t operator()(v32 const& v) const noexcept
		{
			const uint64_t* data = reinterpret_cast<const uint64_t*>(v.data);
			return data[0] ^ data[1] ^ data[2] ^ data[3];
		}
	};
}
