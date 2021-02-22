#pragma once
#include <inttypes.h>

bool hex2bin(const char* in, unsigned int len, unsigned char* out);
void bin2hex(const unsigned char* in, unsigned int len, char* out);
