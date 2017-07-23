#include <iostream>
#include <vector>
#include <thread>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include "file_handler.h"

using namespace boost;

template <typename ConnectionHandler>
class asio_generic_server
{
using shared_handler_t = std::shared_ptr<ConnectionHandler>;
public:

	asio_generic_server(int thread_count=1) 
	: thread_count_(thread_count)
	, acceptor_(io_service_)
	{}

	void start_server(uint16_t port)
	{
		auto handler = std::make_shared<ConnectionHandler>(io_service_);

		// set up the acceptor to listen on the tcp port
		asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
		acceptor_.open(endpoint.protocol());
		acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
		acceptor_.bind(endpoint);
		acceptor_.listen();

		acceptor_.async_accept( handler->socket(), [=](auto ec)
			{
				handle_new_connection(handler, ec);
			});

		// start pool of threads to process the asio events
		for (int i = 0; i < thread_count_; ++i) {
			thread_pool_.emplace_back( [=]{io_service_.run();} );
		}

		for (int i = 0; i < thread_count_; ++i) {
			thread_pool_[i].join();
		}

	}

private:

	void handle_new_connection( shared_handler_t handler, system::error_code const &error)
	{
		if (error) { return; }

		handler->start();

		auto new_handler = std::make_shared<ConnectionHandler>(io_service_);

		acceptor_.async_accept( new_handler->socket(), [=](auto ec)
			{
				handle_new_connection(new_handler, ec);
			});
	}

	int thread_count_;
	std::vector<std::thread> thread_pool_;
	asio::io_service io_service_;
	asio::ip::tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
	using boost::lexical_cast;
    using boost::bad_lexical_cast;

	uint16_t port = 8282;
	if (argc > 1) {
		try {
			port = lexical_cast<uint16_t>(argv[1]);
		}
		catch(bad_lexical_cast &) {
			std::cout << "Error port argument! Use default - 8282" << std::endl;
			// port = 8282
		}
	}

	std::cout << "Connected to port = " << port << std::endl;

	asio_generic_server<file_handler> server{3};

	server.start_server(port);

}
