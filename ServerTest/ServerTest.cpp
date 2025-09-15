#include <iostream>
#include "CServer.h"
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>

using namespace std;

// 这是一个使用strand的类
class MyHandler {
public:
	MyHandler(boost::asio::io_context& io_context)
		: strand_(io_context.get_executor()) {
		std::cout << "MyHandler created." << std::endl;
	}
private:
	boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};

int main()
{
	try {
		boost::asio::io_context io_context;
		MyHandler handler(io_context);
		std::cout << "Test passed." << std::endl;
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	getchar();
	return 0;
    //try {
    //    boost::asio::io_context  io_context;
    //    CServer s(io_context, 10086);
    //    io_context.run();
    //}
    //catch (std::exception& e) {
    //    std::cerr << "Exception: " << e.what() << endl;
    //}
    //boost::asio::io_context io_context;
}