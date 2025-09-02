#include "CSession.h"
#include "CServer.h"
#include <iostream>

CSession::CSession(boost::asio::io_context& io_context, CServer* server) 
	:_socket(io_context),_server(server), _b_close(false)
{
	// ����һ��ȫ��Ψһ��ʶ�� (UUID)
	boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);
	// ��ʼ�����ջ���������Ϣͷ��
	_recv_head_node = make_shared<MsgNode>(HEAD_LENGTH);
}

CSession::~CSession()
{
	cout << "~CSession destruct" << endl;
}

tcp::socket& CSession::GetSocket()
{
	return _socket;
}

const std::string& CSession::GetUuid() const
{
	return _uuid;
}

void CSession::Start()
{
	::memset(_data, 0, MAX_LENGTH);
	_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
		std::bind(&CSession::HandleRead, this, std::placeholders::_1,
			std::placeholders::_2, SharedSelf()));
}

void CSession::Close()
{
	boost::system::error_code ec;
	_socket.close(ec);
	_b_close = true;
}

void CSession::Send(char* msg, int max_length) {
	bool pending = false;
	std::lock_guard<std::mutex> lock(_send_lock);
	if (_send_que.size() > 0) {
		pending = true;
	}
	_send_que.push(make_shared<MsgNode>(msg, max_length));
	if (pending) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

std::shared_ptr<CSession> CSession::SharedSelf()
{
	return shared_from_this();
}

void CSession::HandleRead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> shared_self)
{
	if (!error)
	{
		// �Ѿ���������ֽ�����ָ��ƫ������
		int copy_len = 0;
		// ����δ���������
		while (bytes_transferred > 0)
		{
			if (!_b_head_parse)//δ��������Ϣͷ
			{
				// ���1: ���յ������� + ֮ǰ��������ݣ��Բ���һ������ͷ��
				if (bytes_transferred + _recv_head_node->_cur_len < HEAD_LENGTH)
				{
					// ���ݲ�������ͷ����ֱ�ӻ���
					memcpy(_recv_head_node->_data + _recv_head_node->_cur_len, _data + copy_len, bytes_transferred);
					_recv_head_node->_cur_len += bytes_transferred;

					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&CSession::HandleRead, this, std::placeholders::_1,
							std::placeholders::_2, shared_self));
					return;// ���δ������
				}

				// ���2: ���յ��������㹻����������ͷ��
				// ���㻹��Ҫ�����ֽڲ��ܲ���ͷ��
				int head_remain = HEAD_LENGTH - _recv_head_node->_cur_len;
				memcpy(_recv_head_node->_data + _recv_head_node->_cur_len, _data + copy_len, head_remain);

				//�����Ѵ����data���Ⱥ�ʣ��δ����ĳ���
				copy_len += head_remain;
				bytes_transferred -= head_remain;

				// ������Ϣ�峤��
				short data_len = 0;
				memcpy(&data_len, _recv_head_node->_data, HEAD_LENGTH);
				//�����ֽ���ת��Ϊ�����ֽ���
				data_len = boost::asio::detail::socket_ops::network_to_host_short(data_len);
				cout << "data_len is : " << data_len << endl;

				// ����У��:ͷ�����ȷǷ�
				if (data_len > MAX_LENGTH)
				{
					std::cout << "invalid data length is " << data_len << endl;
					_server->ClearSession(_uuid);
					return;
				}

				// ���2.1: ���յ������ݲ���һ��������Ϣ�壨�������ˡ��������
				_recv_msg_node = make_shared<MsgNode>(data_len);
				//��Ϣ�ĳ���С��ͷ���涨�ĳ��ȣ�˵������δ��ȫ�����Ƚ�������Ϣ�ŵ����սڵ���
				if (bytes_transferred < data_len)
				{
					// ������ʣ�����ݿ�������Ϣ�建����
					memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, bytes_transferred);
					_recv_msg_node->_cur_len += bytes_transferred;
					// ���·����첽���������ȴ�ʣ������
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&CSession::HandleRead, this, std::placeholders::_1,
							std::placeholders::_2, shared_self));
					//ͷ���������
					_b_head_parse = true;
					return;
				}

				// ���2.2: ���յ��������㹻һ��������Ϣ�壬��������ʣ�����ݣ��������ˡ�ճ������
				// ���ݹ�������Ϣ�壬ֱ�ӿ���
				memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, data_len);
				_recv_msg_node->_cur_len += data_len;
				copy_len += data_len;
				bytes_transferred -= data_len;
				_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
				//�˴����Ե���send���Ͳ���
				// TODO: ������ҵ�����߼�Ӧ��������
				Send(_recv_msg_node->_data, _recv_msg_node->_total_len);
				// ����״̬,������ѯʣ��δ��������
				_b_head_parse = false;
				_recv_head_node->Clear();

				// �������ʣ�����ݣ�����ѭ��
				if (bytes_transferred <= 0)
				{
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data,MAX_LENGTH),
						std::bind(&CSession::HandleRead,this,std::placeholders::_1,
							std::placeholders::_2, shared_self));
					return;
				}
				continue;// �������ݣ����� while ѭ��
			}
			// ���3: ���յ��������Բ����Բ���ʣ����Ϣ��
			int remain_msg = _recv_msg_node->_total_len - _recv_msg_node->_cur_len;
			if (bytes_transferred < remain_msg)
			{
				// ����������׷�ӵ���Ϣ�建����
				memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, bytes_transferred);
				_recv_msg_node->_cur_len += bytes_transferred;
				// ���·����첽������
				::memset(_data, 0, MAX_LENGTH);
				_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
					std::bind(&CSession::HandleRead, this, std::placeholders::_1,
						std::placeholders::_2, shared_self));
				return;
			}

			// ���4: ���յ��������㹻����ʣ����Ϣ��
			memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, remain_msg);
			_recv_msg_node->_cur_len += remain_msg;
			bytes_transferred -= remain_msg;
			copy_len += remain_msg;
			_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
			cout << "receive data is " << _recv_msg_node->_data << endl;
			// TODO: ������ҵ�����߼�
			//�˴����Ե���Send���Ͳ���
			Send(_recv_msg_node->_data, _recv_msg_node->_total_len);
			// ����״̬,������ѯʣ��δ��������
			_b_head_parse = false;
			_recv_head_node->Clear();
			if (bytes_transferred <= 0) {
				::memset(_data, 0, MAX_LENGTH);
				_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
					std::bind(&CSession::HandleRead, this, std::placeholders::_1, std::placeholders::_2, shared_self));
				return;
			}
			continue;
		}
	}
	else
	{
	// �첽������ʧ�ܣ���ӡ������Ϣ���ر�����
		std::cout << "handle read failed, error is " << error.what() << endl;
		Close();
		_server->ClearSession(_uuid);
	}
}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self)
{
	if (!error)
	{
		std::lock_guard<std::mutex> lock(_send_lock);
		if (_send_que.empty())
		{
			std::cerr << "Warning: HandleWrite called but send queue is empty\n";
			return;
		}
		auto msgnode = _send_que.front();
		_send_que.pop();
		// ��ӡ���ݣ��������ַ���������������룩
		cout << "send data:" << 
			std::string(msgnode->_data+HEAD_LENGTH,msgnode->_total_len-HEAD_LENGTH) << endl;
		if (!_send_que.empty())
		{
			auto& msgnode = _send_que.front();
			/*boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
				std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_self));*/
			boost::asio::async_write(
				_socket,
				boost::asio::buffer(msgnode->_data, msgnode->_total_len),
				[shared_self](const boost::system::error_code& ec, std::size_t) {
					shared_self->HandleWrite(ec, shared_self);
				}
			);
		}
	}
	else
	{
		std::cout << "handle write failed, error is " << error.message() << endl;
		Close();
		_server->ClearSession(_uuid);
	}
}

