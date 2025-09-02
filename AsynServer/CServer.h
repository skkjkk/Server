#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include <map>
#include <memory.h>
#include "CSession.h"

using namespace std;
using boost::asio::ip::tcp;

class CServer {
public:
	CServer(boost::asio::io_context& io_context, short port);
	void ClearSession(std::string);
private:
	void StartAccept();
	void HandleAccept(shared_ptr<CSession>, const boost::system::error_code& error);
	boost::asio::io_context& _io_context;
	short _port;
	tcp::acceptor _acceptor;
	std::map<std::string, shared_ptr<CSession>> _sessions;
};