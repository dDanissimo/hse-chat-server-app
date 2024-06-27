#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <array>
#include <boost/asio.hpp>
#include <deque>
#include <string>

constexpr int MAX_NICKNAME = 16;
constexpr int MAX_IP_PACK_SIZE = 512;
constexpr int PADDING = 24;

using boost::asio::ip::tcp;

class client {
public:
    client(const std::array<char, MAX_NICKNAME>& nickname,
           boost::asio::io_service& io_service,
           tcp::resolver::iterator endpoint_iterator);

    void write(const std::array<char, MAX_IP_PACK_SIZE>& msg);
    void close();
    void onConnect(const boost::system::error_code& error);

private:
    void readHandler(const boost::system::error_code& error);
    void writeImpl(std::array<char, MAX_IP_PACK_SIZE> msg);
    void writeHandler(const boost::system::error_code& error);
    void closeImpl();

    boost::asio::io_service& io_service_;
    tcp::socket socket_;
    std::array<char, MAX_IP_PACK_SIZE> read_msg_;
    std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_msgs_;
    std::array<char, MAX_NICKNAME> nickname_;
};

#endif // CLIENT_HPP
