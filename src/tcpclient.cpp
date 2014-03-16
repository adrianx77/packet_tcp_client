#include "tcpclient.h"
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/interprocess/detail/atomic.hpp>
namespace adrianx
{
	tcp_client::tcp_client(): _error_state(0), _shutdown( 1 ),_recv_pack_size(0)
	{
	}

	tcp_client::~tcp_client()
	{
		close();
	}

	bool tcp_client::connect( const std::string & host, uint16_t port )
	{

		if(boost::interprocess::ipcdetail::atomic_cas32( &_shutdown, 0, 1 )==0)//�Ѿ���������״̬
			return false;

		bool ok = false;
		boost::asio::ip::tcp::resolver::iterator iterator;
		do 
		{
			_io_service_ptr = io_service_sptr(new boost::asio::io_service());
			if(!_io_service_ptr)
				break;
			_work_ptr = work_sptr(new boost::asio::io_service::work(*_io_service_ptr));
			if(!_work_ptr)
				break;
			_socket = socket_sptr(new boost::asio::ip::tcp::socket(*_io_service_ptr));
			if(!_socket)
				break;
			_strand = strand_sptr(new boost::asio::strand(*_io_service_ptr));
			if(!_strand)
				break;

			boost::system::error_code ec;
			boost::asio::ip::tcp::resolver resolver( *_io_service_ptr );
			boost::asio::ip::tcp::resolver::query query( host, boost::lexical_cast< std::string >( port ) );
			iterator = resolver.resolve( query );
			
			_thread_ptr = thread_sptr(new boost::thread(boost::bind( &boost::asio::io_service::run, _io_service_ptr)));
			if(!_thread_ptr)
				break;
			ok = true;
		} while (0);

		if(!ok)
		{
			_strand.reset();
			_socket.reset();
			_work_ptr.reset();
			_io_service_ptr.reset();
			boost::interprocess::ipcdetail::atomic_cas32( &_shutdown, 1, 0 );
			return false;
		}
		_socket->async_connect( *iterator, _strand->wrap( boost::bind(  &tcp_client::handle_connect, shared_from_this(),  boost::asio::placeholders::error ) ) );
		return true;
	}

	void tcp_client::close()
	{
		if( boost::interprocess::ipcdetail::atomic_cas32( &_shutdown, 1, 0 ) == 0 )
		{
			_strand->post( boost::bind( &tcp_client::handle_error, shared_from_this(), boost::asio::error::connection_reset));
			_thread_ptr->join();

			_thread_ptr.reset();
			_work_ptr.reset();
			_socket.reset();
			_strand.reset();
			_io_service_ptr.reset();
			clear_pending_send();
		}
	}

	bool tcp_client::send( uint8_t * data,uint16_t size )
	{
		if（data == 0)
			return false;
		
		if(size == 0)
			return false;
		
		
		if(has_stopped())
			return false;

		if(push_pending_send(data,size))
			_strand->post(boost::bind(&tcp_client::start_send,shared_from_this()));

		return true;
	}

	void tcp_client::handle_error( const boost::system::error_code & error )
	{
		if( boost::interprocess::ipcdetail::atomic_cas32( &_error_state, 1, 0 ) == 0 )
		{
			boost::system::error_code ec;
			_socket->shutdown( boost::asio::ip::tcp::socket::shutdown_both, ec );
			_socket->close( ec );
			_io_service_ptr->reset();
			_io_service_ptr->stop();
			on_error( error );
		}
	}

	bool tcp_client::has_error()
	{
		return ( boost::interprocess::ipcdetail::atomic_cas32( &_error_state, 1, 1 ) == 1 );	
	}

	void tcp_client::handle_connect( const boost::system::error_code & error )
	{
		if( error || has_error() ||has_stopped())
		{
			handle_error( error );
		}
		else
		{
			if( _socket->is_open() )
			{
				on_connected( _socket->remote_endpoint().address().to_string(), _socket->remote_endpoint().port(),
					_socket->local_endpoint().port()
					);

				_socket->async_receive(boost::asio::buffer( &_recv_pack_size ,sizeof(_recv_pack_size)),
					boost::bind(&tcp_client::handle_header_recv,shared_from_this(), boost::asio::placeholders::error));
			}
			else
			{
				handle_error( error );
			}
		}
	}

	void tcp_client::handle_header_recv( const boost::system::error_code & error )
	{
		if( error || has_error()||has_stopped())
		{
			handle_error( error );
		}
		else
		{
#if(__BYTE_ORDER==__LITTLE_ENDIAN)	
			_recv_pack_size = ((_recv_pack_size & 0xff00) >> 8) | ((_recv_pack_size & 0xff) <<8);
#endif
			_socket->async_receive(boost::asio::buffer(_recv_body,_recv_pack_size),boost::bind(&tcp_client::handle_body_recv,shared_from_this(), boost::asio::placeholders::error));
		}
	}

	void tcp_client::handle_body_recv( const boost::system::error_code & error )
	{
		if( error || has_error()||has_stopped())
		{
			handle_error( error );
		}
		else
		{
			std::vector<uint8_t> buffer(_recv_body,_recv_body+_recv_pack_size);

			on_recv(buffer);

			_socket->async_receive(boost::asio::buffer( &_recv_pack_size ,sizeof(_recv_pack_size)),
				boost::bind(&tcp_client::handle_header_recv,shared_from_this(), boost::asio::placeholders::error));
		}
	}

	bool tcp_client::push_pending_send( void * data,uint16_t size )
	{
		if(!size)
			return false;

		uint8_t * pack = new uint8_t[size+2];
		if(!pack)
			return false;
#if(__BYTE_ORDER==__LITTLE_ENDIAN)	
		pack[1] = size&0xff;
		pack[0] = (size&0xff00)>>8;
#else
		pack[0] = size&0xff;
		pack[1] = (size&0xff00)>>8;
#endif	 
		memcpy(pack+2,data,size);
		bool need_start_send = false;
		boost::recursive_mutex::scoped_lock lock(_pending_mtx);
		if(_pending_sends.empty())
			need_start_send = true;
		_pending_sends.push_back(pack);
		return need_start_send;
	}

	void tcp_client::clear_pending_send()
	{
		if(_pending_sends.empty())
			return;
		boost::recursive_mutex::scoped_lock lock(_pending_mtx);
		while (!_pending_sends.empty())
		{
			uint8_t * pack = _pending_sends.front();
			_pending_sends.pop_front();
			delete []pack;
		}
	}

	void tcp_client::start_send()
	{
		if(has_stopped())
			return;
		if(_pending_sends.empty() )
			return;
		boost::recursive_mutex::scoped_lock lock(_pending_mtx);
		uint8_t * pack = _pending_sends.front();
		int size  = 0;
#if(__BYTE_ORDER==__LITTLE_ENDIAN)	
		size|= pack[1];
		size|= pack[0]<<8;
#else
		size|= pack[0];
		size|= pack[1]<<8;
#endif 
		_socket->async_send( boost::asio::buffer(pack,size+2),
								_strand->wrap( boost::bind(  &tcp_client::handle_send, shared_from_this(), 
								boost::asio::placeholders::error, _pending_sends.begin())));
	}

	void tcp_client::handle_send( const boost::system::error_code & error,std::list< uint8_t *>::iterator& itr)
	{
		if( error || has_error() ||has_stopped())
		{
			handle_error(error );
		}
		else
		{
			boost::recursive_mutex::scoped_lock lock(_pending_mtx);
			uint8_t * pack = _pending_sends.front();
			int size  = 0;
#if(__BYTE_ORDER==__LITTLE_ENDIAN)	
			size|= pack[1];
			size|= pack[0]<<8;
#else
			size|= pack[0];
			size|= pack[1]<<8;
#endif 
			on_send(size+2);
			_pending_sends.erase( itr );

			delete []pack;
			start_send();
		}
	}

	bool tcp_client::has_stopped()
	{
		return ( boost::interprocess::ipcdetail::atomic_cas32( &_shutdown, 1, 1 ) == 1 );
	}

}
