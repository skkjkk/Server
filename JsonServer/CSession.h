#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include "const.h"
#include "MsgNode.h"

using namespace std;

using boost::asio::ip::tcp;
class CServer;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession(boost::asio::io_context& io_context, CServer* server);
	~CSession();
	tcp::socket& GetSocket();
	std::string& GetUuid();
	void Start();
	void Send(char* msg, int max_length, short msg_id);
	void Send(std::string msg, short msg_id);
	void Close();
	std::shared_ptr<CSession> SharedSelf();
private:
	void HandleRead(const boost::system::error_code& error, size_t  bytes_transferred, std::shared_ptr<CSession> shared_self);
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);
	tcp::socket _socket;
	std::string _uuid;
	char _data[MAX_LENGTH];
	CServer* _server;
	bool _b_close;
	std::queue<shared_ptr<SendNode> > _send_que;
	std::mutex _send_lock;
	//�յ�����Ϣ�ṹ
	std::shared_ptr<RecvNode> _recv_msg_node;
	bool _b_head_parse;
	//�յ���ͷ���ṹ
	std::shared_ptr<MsgNode> _recv_head_node;
};

//��װ�˻Ự�������Ϣ���ݣ���Ϊ������㴫�ݵ�ҵ�������ݰ�װ��
class LogicNode
{
	friend class LogicSystem;
public:
	LogicNode(std::shared_ptr<CSession>, std::shared_ptr<RecvNode>);
private:
	std::shared_ptr<CSession> _session;	//�Ự���ã����ڻظ���Ϣ
	std::shared_ptr<RecvNode> _recvnode;//���յ�����Ϣ����
};

