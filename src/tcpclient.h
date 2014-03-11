#ifndef _adrianx_packet_tcp_client_h_
#define _adrianx_packet_tcp_client_h_

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>
#include <list>
#include <boost/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>  
#ifndef __BYTE_ORDER
#define __BIG_ENDIAN		4321
#define __LITTLE_ENDIAN		1234

#if (('4321'>>24)=='1')	/*big*/
#define __BYTE_ORDER __BIG_ENDIAN
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#endif

namespace adrianx
{
	class tcp_client:public boost::enable_shared_from_this<tcp_client>
	{
	private:
		tcp_client(tcp_client&);
		tcp_client& operator =(tcp_client&);


	protected:
		typedef boost::shared_ptr<boost::asio::io_service> io_service_sptr;
		typedef boost::shared_ptr<boost::asio::io_service::work> work_sptr;
		typedef boost::shared_ptr<boost::thread> thread_sptr;
		typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_sptr;
		typedef boost::shared_ptr<boost::asio::strand>	strand_sptr;

		volatile uint32_t					_shutdown;
		thread_sptr							_thread_ptr;
		io_service_sptr						_io_service_ptr;
		work_sptr							_work_ptr;
		socket_sptr							_socket;
		strand_sptr							_strand;


		boost::recursive_mutex				_pending_mtx;  
		std::list< uint8_t *>				_pending_sends;
		uint16_t							_recv_pack_size;	//header
		uint8_t								_recv_body[0x10000];
		
		virtual void on_connected(const std::string & host, uint16_t port,uint16_t lport){
			std::cout<< "connected local port:"<<lport<<std::endl;
		};

		virtual void on_error(const boost::system::error_code & error){};
		virtual void on_closed(){};
		virtual void on_send(uint16_t bytes_sent){};
		virtual void on_recv(std::vector< uint8_t > & buffer){

			std::cout<< "buffer ="<< buffer.size() <<std::endl;
            
            for(int i=0;i<buffer.size();i++)
            {
                printf("0x%02X ",buffer[i]);
            }
            
            std::cout<<std::endl<<"<--------end of packet"<<std::endl;

		};

		void handle_connect(const boost::system::error_code & error );
		void handle_header_recv( const boost::system::error_code & error);
		void handle_body_recv(const boost::system::error_code & error);
		void handle_send( const boost::system::error_code & error,std::list< uint8_t *>::iterator& itr);
		volatile uint32_t _error_state;
		void handle_error(const boost::system::error_code & error);
		bool has_error();
		bool has_stopped();

		void start_send();
		bool push_pending_send(void * data,uint16_t size);
		void clear_pending_send();
	public:
		tcp_client();
		virtual ~tcp_client();

		bool connect(const std::string & host, uint16_t port);
		void close();
		bool send(uint8_t * data,uint16_t size );
	};
}

#endif//_adrianx_packet_tcp_client_h_
