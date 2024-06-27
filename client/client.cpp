#include "client.hpp"
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <cstring>
#include <iostream>

using namespace boost::placeholders;

client::client(const std::array<char, MAX_NICKNAME> &nickname,
               boost::asio::io_service &io_service,
               tcp::resolver::iterator endpoint_iterator)
    : io_service_(io_service), socket_(io_service) {
  if (nickname[0] == '\0') {
    throw std::runtime_error("Empty nickname is not allowed");
  }
  strcpy(nickname_.data(), nickname.data());
  memset(read_msg_.data(), '\0', MAX_IP_PACK_SIZE);
  boost::asio::async_connect(socket_, endpoint_iterator,
                             boost::bind(&client::onConnect, this, _1));
}

void client::write(const std::array<char, MAX_IP_PACK_SIZE> &msg) {
  io_service_.post(boost::bind(&client::writeImpl, this, msg));
}

void client::close() {
  io_service_.post(boost::bind(&client::closeImpl, this));
}

void client::onConnect(const boost::system::error_code &error) {
  if (!error) {
    boost::asio::async_write(socket_,
                             boost::asio::buffer(nickname_, nickname_.size()),
                             boost::bind(&client::readHandler, this, _1));
  } else {
    throw std::runtime_error("Connection failed");
  }
}

void client::readHandler(const boost::system::error_code &error) {
  std::cout << read_msg_.data() << std::endl;
  if (!error) {
    boost::asio::async_read(socket_,
                            boost::asio::buffer(read_msg_, read_msg_.size()),
                            boost::bind(&client::readHandler, this, _1));
  } else {
    closeImpl();
  }
}

void client::writeImpl(std::array<char, MAX_IP_PACK_SIZE> msg) {
  bool write_in_progress = !write_msgs_.empty();
  write_msgs_.push_back(msg);
  if (!write_in_progress) {
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
        boost::bind(&client::writeHandler, this, _1));
  }
}

void client::writeHandler(const boost::system::error_code &error) {
  if (!error) {
    write_msgs_.pop_front();
    if (!write_msgs_.empty()) {
      boost::asio::async_write(
          socket_,
          boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
          boost::bind(&client::writeHandler, this, _1));
    }
  } else {
    closeImpl();
  }
}

void client::closeImpl() { socket_.close(); }

#ifndef UNIT_TEST
int main(int argc, char *argv[]) {
  try {
    if (argc != 4) {
      std::cerr << "Usage: " << argv[0] << " <username> <host> <port>\n";
      return 1;
    }

    std::string username = argv[1];
    if (username.length() >= MAX_NICKNAME) {
      std::cerr << "Username is too long. Maximum length is "
                << MAX_NICKNAME - 1 << " characters.\n";
      return 1;
    }

    boost::asio::io_service io_service;

    tcp::resolver resolver(io_service);
    tcp::resolver::query query(argv[2], argv[3]);
    tcp::resolver::iterator iterator = resolver.resolve(query);

    std::array<char, MAX_NICKNAME> nickname;
    std::fill(nickname.begin(), nickname.end(), '\0');
    std::copy(username.begin(), username.end(), nickname.begin());

    client c(nickname, io_service, iterator);

    std::thread t([&io_service]() { io_service.run(); });

    char line[MAX_IP_PACK_SIZE + 1];
    while (std::cin.getline(line, MAX_IP_PACK_SIZE + 1)) {
      std::array<char, MAX_IP_PACK_SIZE> msg;
      std::fill(msg.begin(), msg.end(), 0);
      std::memcpy(msg.data(), line, std::strlen(line));
      c.write(msg);
    }

    c.close();
    t.join();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
#endif