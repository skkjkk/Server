/*
main Ϊ���̣߳�������io_contextʵ������������һ�������߳�net_work_thread��
���߳��д����˸�CServerʵ����������ioc.run()���������������̣������¼�ѭ����
�������߳�ע���źŴ�����sig_handler������whileѭ��������cond_quit.wait(lock_quit)��
�����߳���������������������ʱ���̵߳ȴ���
һ����⵽�˳��ź�ʱsig_handler������ִ�У���ȡ����������ȫ�޸�bstop״̬Ϊ
true ����������֪ͨ���������̡߳�
���̱߳�����֮���ж�whileѭ��������Ϊ�����˳�ѭ��������ioc.stop()������ʹrun()
�������̷��أ����������ֳɵ��¼�ѭ����Ȼ��net_work_thread.join()�������̣߳�
ֱ��net_work_thread����ִ����ϲ��˳���join()�������أ������򱻻��Ѽ���ִ��try�顣
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

//�����źŵĻص�����
//void sig_handler(int sig)
//{
//	if (sig == SIGINT || sig == SIGTERM)
//	{
//		std::unique_lock<std::mutex> lock(mutex_quit);
//		bstop = true;
//		cond_quit.notify_one();//�������߳�
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
		//	cond_quit.wait(lock_quit);//���̱߳��������ȴ���������֪ͨ���ѣ����Ѻ���õ�����
	}
		/*io_context.stop();
		net_work_thread.join();*/
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << endl;
	}
}