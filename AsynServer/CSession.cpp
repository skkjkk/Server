#include "CSession.h"
#include "CServer.h"
#include <iostream>

CSession::CSession(boost::asio::io_context& io_context, CServer* server) 
	:_socket(io_context),_server(server), _b_close(false)
{
	// 生成一个全局唯一标识符 (UUID)
	boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);
	// 初始化接收缓冲区（消息头）
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
		// 已经处理过的字节数（指针偏移量）
		int copy_len = 0;
		// 还有未处理的数据
		while (bytes_transferred > 0)
		{
			if (!_b_head_parse)//未解析出消息头
			{
				// 情况1: 接收到的数据 + 之前缓存的数据，仍不足一个完整头部
				if (bytes_transferred + _recv_head_node->_cur_len < HEAD_LENGTH)
				{
					// 数据不够完整头部，直接缓存
					memcpy(_recv_head_node->_data + _recv_head_node->_cur_len, _data + copy_len, bytes_transferred);
					_recv_head_node->_cur_len += bytes_transferred;

					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&CSession::HandleRead, this, std::placeholders::_1,
							std::placeholders::_2, shared_self));
					return;// 本次处理结束
				}

				// 情况2: 接收到的数据足够解析出完整头部
				// 计算还需要多少字节才能补齐头部
				int head_remain = HEAD_LENGTH - _recv_head_node->_cur_len;
				memcpy(_recv_head_node->_data + _recv_head_node->_cur_len, _data + copy_len, head_remain);

				//更新已处理的data长度和剩余未处理的长度
				copy_len += head_remain;
				bytes_transferred -= head_remain;

				// 解析消息体长度
				short data_len = 0;
				memcpy(&data_len, _recv_head_node->_data, HEAD_LENGTH);
				//网络字节序转化为本地字节序
				data_len = boost::asio::detail::socket_ops::network_to_host_short(data_len);
				cout << "data_len is : " << data_len << endl;

				// 长度校验:头部长度非法
				if (data_len > MAX_LENGTH)
				{
					std::cout << "invalid data length is " << data_len << endl;
					_server->ClearSession(_uuid);
					return;
				}

				// 情况2.1: 接收到的数据不够一个完整消息体（即发生了“半包”）
				_recv_msg_node = make_shared<MsgNode>(data_len);
				//消息的长度小于头部规定的长度，说明数据未收全，则先将部分消息放到接收节点里
				if (bytes_transferred < data_len)
				{
					// 将本次剩余数据拷贝到消息体缓存区
					memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, bytes_transferred);
					_recv_msg_node->_cur_len += bytes_transferred;
					// 重新发起异步读操作，等待剩余数据
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&CSession::HandleRead, this, std::placeholders::_1,
							std::placeholders::_2, shared_self));
					//头部处理完成
					_b_head_parse = true;
					return;
				}

				// 情况2.2: 接收到的数据足够一个完整消息体，甚至还有剩余数据（即发生了“粘包”）
				// 数据够完整消息体，直接拷贝
				memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, data_len);
				_recv_msg_node->_cur_len += data_len;
				copy_len += data_len;
				bytes_transferred -= data_len;
				_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
				//此处可以调用send发送测试
				// TODO: 真正的业务处理逻辑应该在这里
				Send(_recv_msg_node->_data, _recv_msg_node->_total_len);
				// 重置状态,继续轮询剩余未处理数据
				_b_head_parse = false;
				_recv_head_node->Clear();

				// 如果还有剩余数据，继续循环
				if (bytes_transferred <= 0)
				{
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data,MAX_LENGTH),
						std::bind(&CSession::HandleRead,this,std::placeholders::_1,
							std::placeholders::_2, shared_self));
					return;
				}
				continue;// 还有数据，继续 while 循环
			}
			// 情况3: 接收到的数据仍不足以补齐剩余消息体
			int remain_msg = _recv_msg_node->_total_len - _recv_msg_node->_cur_len;
			if (bytes_transferred < remain_msg)
			{
				// 将本次数据追加到消息体缓存区
				memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, bytes_transferred);
				_recv_msg_node->_cur_len += bytes_transferred;
				// 重新发起异步读操作
				::memset(_data, 0, MAX_LENGTH);
				_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
					std::bind(&CSession::HandleRead, this, std::placeholders::_1,
						std::placeholders::_2, shared_self));
				return;
			}

			// 情况4: 接收到的数据足够补齐剩余消息体
			memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, remain_msg);
			_recv_msg_node->_cur_len += remain_msg;
			bytes_transferred -= remain_msg;
			copy_len += remain_msg;
			_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
			cout << "receive data is " << _recv_msg_node->_data << endl;
			// TODO: 真正的业务处理逻辑
			//此处可以调用Send发送测试
			Send(_recv_msg_node->_data, _recv_msg_node->_total_len);
			// 重置状态,继续轮询剩余未处理数据
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
	// 异步读操作失败，打印错误信息并关闭连接
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
		// 打印数据（假设是字符串，否则可能乱码）
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

