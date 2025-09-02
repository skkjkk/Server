#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <iostream>
#include <memory>
#include <queue>
#include <mutex>

#include "const.h"

using namespace std;
using boost::asio::ip::tcp;

class CServer;

//消息单元
class MsgNode
{
	friend class CSession;
public:
	//构造函数
	MsgNode(char* msg, short max_len) : _total_len(max_len + HEAD_LENGTH), _cur_len(0) 
	{
		_data = new char[_total_len + 1]();//分配内存
		//转为网络字节序
		int max_len = boost::asio::detail::socket_ops::host_to_network_short(max_len);
		//&max_len不是写地址，而是告诉 memcpy从 max_len 的内存里取值
		memcpy(_data, &max_len, HEAD_LENGTH);//前 2 字节存放消息长度
		memcpy(_data + HEAD_LENGTH, msg, max_len);// 拷贝消息内容
		_data[_total_len] = '\0';
	}
	//构造函数,用于接收消息
	MsgNode(short max_len):_total_len(max_len),_cur_len(0)
	{
		_data = new char[_total_len + 1]();
	}

	~MsgNode() {
		delete[] _data;
	}

	void Clear() 
	{
		::memset(_data, 0, _total_len);
		_cur_len = 0;
	}
private:
	short _cur_len;// 已经接收的字节数
	short _total_len;// 总字节数（头部+消息体）
	char* _data;// 存放消息的缓冲区
};

class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession(boost::asio::io_context& io_context, CServer* server);
	~CSession();
	tcp::socket& GetSocket();//获取底层的 socket，用于接收客户端连接
	const std::string& GetUuid() const;//返回会话唯一标识符
	void Start();
	void Close();
	void Send(char* msg, int max_length);
	std::shared_ptr<CSession> SharedSelf();//安全的引用自己，延长会话周期
private:
	void HandleRead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> shared_self);
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);
	tcp::socket _socket;// 会话专属的 socket
	std::string _uuid;// 会话唯一标识符
	char _data[MAX_LENGTH];// 缓冲区
	CServer* _server;// 指向所属服务器
	bool _b_close;// 是否已关闭
	std::queue<shared_ptr<MsgNode>> _send_que;// 待发送的消息队列
	std::mutex _send_lock;// 发送队列互斥锁
	bool _b_head_parse;// 是否已经解析出消息头
	//缓冲区专门用来接收消息头，在 async_read 时，先读 2 个字节（消息长度），
	//再根据长度创建另一个 MsgNode 用于消息体。
	std::shared_ptr<MsgNode> _recv_head_node;// 当前接收的消息头
	//收到的消息结构
	std::shared_ptr<MsgNode> _recv_msg_node;// 当前正在接收的消息
};