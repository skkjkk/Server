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

//��Ϣ��Ԫ
class MsgNode
{
	friend class CSession;
public:
	//���캯��
	MsgNode(char* msg, short max_len) : _total_len(max_len + HEAD_LENGTH), _cur_len(0) 
	{
		_data = new char[_total_len + 1]();//�����ڴ�
		//תΪ�����ֽ���
		int max_len = boost::asio::detail::socket_ops::host_to_network_short(max_len);
		//&max_len����д��ַ�����Ǹ��� memcpy�� max_len ���ڴ���ȡֵ
		memcpy(_data, &max_len, HEAD_LENGTH);//ǰ 2 �ֽڴ����Ϣ����
		memcpy(_data + HEAD_LENGTH, msg, max_len);// ������Ϣ����
		_data[_total_len] = '\0';
	}
	//���캯��,���ڽ�����Ϣ
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
	short _cur_len;// �Ѿ����յ��ֽ���
	short _total_len;// ���ֽ�����ͷ��+��Ϣ�壩
	char* _data;// �����Ϣ�Ļ�����
};

class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession(boost::asio::io_context& io_context, CServer* server);
	~CSession();
	tcp::socket& GetSocket();//��ȡ�ײ�� socket�����ڽ��տͻ�������
	const std::string& GetUuid() const;//���ػỰΨһ��ʶ��
	void Start();
	void Close();
	void Send(char* msg, int max_length);
	std::shared_ptr<CSession> SharedSelf();//��ȫ�������Լ����ӳ��Ự����
private:
	void HandleRead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> shared_self);
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);
	tcp::socket _socket;// �Ựר���� socket
	std::string _uuid;// �ỰΨһ��ʶ��
	char _data[MAX_LENGTH];// ������
	CServer* _server;// ָ������������
	bool _b_close;// �Ƿ��ѹر�
	std::queue<shared_ptr<MsgNode>> _send_que;// �����͵���Ϣ����
	std::mutex _send_lock;// ���Ͷ��л�����
	bool _b_head_parse;// �Ƿ��Ѿ���������Ϣͷ
	//������ר������������Ϣͷ���� async_read ʱ���ȶ� 2 ���ֽڣ���Ϣ���ȣ���
	//�ٸ��ݳ��ȴ�����һ�� MsgNode ������Ϣ�塣
	std::shared_ptr<MsgNode> _recv_head_node;// ��ǰ���յ���Ϣͷ
	//�յ�����Ϣ�ṹ
	std::shared_ptr<MsgNode> _recv_msg_node;// ��ǰ���ڽ��յ���Ϣ
};