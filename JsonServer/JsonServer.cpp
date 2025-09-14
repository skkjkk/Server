/*
main 为主线程，创建了io_context实例，并启动了一个工作线程net_work_thread，
该线程中创建了个CServer实例，并调用ioc.run()方法阻塞工作流程，进入事件循环。
接着主线程注册信号处理器sig_handler，进入while循环并调用cond_quit.wait(lock_quit)，
是主线程在条件变量上阻塞，此时主线程等待。
一旦检测到退出信号时sig_handler函数被执行，获取到互斥锁安全修改bstop状态为
true ，条件变量通知并唤醒主线程。
主线程被唤醒之后，判断while循环条件，为假则退出循环。调用ioc.stop()方法，使run()
方法立刻返回，结束工作现成的事件循环。然后net_work_thread.join()阻塞主线程，
直到net_work_thread程序执行完毕并退出，join()方法返回，主程序被唤醒继续执行try块。
*/
#include "CServer.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <mutex>

using namespace std;
bool bstop = false;
std::mutex mutex_quit;
std::condition_variable cond_quit;

//处理信号的回调函数
//void sig_handler(int sig)
//{
//	if (sig == SIGINT || sig == SIGTERM)
//	{
//		std::unique_lock<std::mutex> lock(mutex_quit);
//		bstop = true;
//		cond_quit.notify_one();//激活主线程
//	}
//}
#include "AsioIOServicePool.h"
int main()
{
	try {
		auto pool = AsioIOServicePool::GetInstance();
		boost::asio::io_context  io_context;
		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait([&io_context,pool](auto, auto) {
			io_context.stop();
			pool->Stop();
			});
		CServer s(io_context, 10086);
		io_context.run();
		//std::thread net_work_thread([&io_context]() 
		//	{
		//		CServer s(io_context, 10086);
		//		io_context.run();
		//	});

		//std::signal(SIGINT, sig_handler);
		//std::signal(SIGTERM, sig_handler);

		//while (!bstop)
		//{
		//	std::unique_lock<std::mutex> lock_quit(mutex_quit);
		//	cond_quit.wait(lock_quit);//主线程被阻塞，等待条件变量通知唤醒（唤醒后会拿到锁）
	}
		/*io_context.stop();
		net_work_thread.join();*/
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << endl;
	}
}