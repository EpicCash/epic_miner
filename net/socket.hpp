#pragma once

#include "socks.hpp"
#include <atomic>
#include <iostream>
#include <string>
#include <vector>

class base_socket
{
public:
	virtual ~base_socket() = default;
	virtual bool set_hostname(const char* sAddr) = 0;
	virtual bool connect() = 0;
	virtual int recv(char* buf, unsigned int len) = 0;
	virtual bool send(const char* buf, size_t len) = 0;
	virtual void close(bool free) = 0;
	inline void print_error_s(const char* err_msg);

protected:
	std::atomic<bool> sock_closed;
};

class plain_socket : public base_socket
{
  public:
	plain_socket();

	bool set_hostname(const char* sAddr);
	bool connect();
	int recv(char* buf, unsigned int len);
	bool send(const char* buf, size_t len);
	void close(bool free);
	void print_error();
	void print_error(int gai_err);

  private:
	addrinfo* pSockAddr;
	addrinfo* pAddrRoot;
	SOCKET hSocket;
};

typedef struct ssl_ctx_st SSL_CTX;
typedef struct bio_st BIO;
typedef struct ssl_st SSL;

class tls_socket : public base_socket
{
  public:
	tls_socket() {};

	bool set_hostname(const char* sAddr);
	bool connect();
	int recv(char* buf, unsigned int len);
	bool send(const char* buf, size_t len);
	void close(bool free);

  private:
	void init_ctx();
	void print_error();

	SSL_CTX* ctx = nullptr;
	BIO* bio = nullptr;
	SSL* ssl = nullptr;
};
