#include <iostream>
#include <fstream>
#include <deque>
#include <boost/asio.hpp>


using namespace boost;

void LOG(const std::string& msg) {
    std::cout << msg << std::endl;
}


class file_handler : public std::enable_shared_from_this<file_handler>
{
public:

    file_handler(asio::io_service& service)
    : service_(service)
    , socket_(service)
    , write_strand_(service)
    {}

    asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    void start()
    {
        read_packet();
    }

    void send(std::string msg)
    {
        service_.post( write_strand_.wrap([me=shared_from_this(), msg]()
            {
                me->queue_message(msg);
            }) );
    }

private:

    void queue_message(std::string message)
    {
        bool write_in_progress = !send_packet_queue.empty();
        send_packet_queue.push_back(message);

        if (!write_in_progress) {
            start_packet_send();
        }
    }

    void start_packet_send()
    {
        async_write( socket_, asio::buffer(send_packet_queue.front()), write_strand_.wrap(
            [me=shared_from_this()](system::error_code const & ec, std::size_t){
                me->packet_send_done(ec);
            }));
    }

    void packet_send_done(system::error_code const & error)
    {
        if (!error) {
            send_packet_queue.pop_front();

            if (!send_packet_queue.empty()) {
                start_packet_send();
                return;
            }

            if (file.good() && !file.eof()) {
                const int size = 4; // для теста, можно поставить больше значение
                char buf[size];
                memset(buf, 0, size);
                file.read(buf, size);
                std::string msg(buf, size);
                send(msg);
            }
            else if (send_packet_queue.empty()) {
                file.close();
                socket_.close();
            }
        }
    }

    void read_packet()
    {
        asio::async_read_until( socket_, in_packet_, "\r\n\r\n", 
            [me=shared_from_this()](system::error_code const & ec, std::size_t bytes_xfer)
            {
                me->read_packet_done(ec, bytes_xfer);
            });
    }

    void read_packet_done(system::error_code const & error, std::size_t bytes_transferred)
    {
        if (error) {return;}

        std::istream stream(&in_packet_);
        std::string packet_string;
        stream >> packet_string;

        if ( packet_string == "GET" ) {
            read_packet();
        }
        else {
            std::string filename;
            std::string start = "/get/";

            if (packet_string.find(start) != std::string::npos) {
                filename = packet_string.substr(start.size());
            }
            file.open(filename, std::ios::binary | std::ios::ate);

            if (file.good()) {
                std::streamsize filesize = file.tellg();
                file.seekg(0, std::ios::beg);
                
                send(headerFile(filename, filesize));
            }
            else {
                send(headerError(packet_string));
            }
        }
    }

    std::string headerFile(const std::string& filename, int contentLength)
    {
        std::string h =
        "HTTP/1.1 200 OK\r\n"
        "Content-Description: File Transfer\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename="+filename+"\r\n"
        "Content-Transfer-Encoding: binary\r\n"
        "Expires: 0\r\n"
        "Cache-Control: must-revalidate\r\n"
        "Pragma: public\r\n"
        "Content-Length: " + boost::lexical_cast<std::string>(contentLength) + "\r\n"
        "\r\n";

        return h;

    }

    std::string headerError(const std::string& error)
    {
        std::string h =
        "HTTP/1.0 404 Not Found\r\n"
        "\r\n\r\n"
        "<html>"
        "<head><title>Not Found</title></head>"
        "<body><h1>404 Not Found "+error+"</h1></body>"
        "</html>";

        return h;
    }

    asio::io_service& service_;
    asio::ip::tcp::socket socket_;
    asio::io_service::strand write_strand_;
    asio::streambuf in_packet_;
    std::deque<std::string> send_packet_queue;
    std::ifstream file;
};
