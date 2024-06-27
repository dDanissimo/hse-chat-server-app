#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <../external/doctest/doctest.h>
#include <boost/asio.hpp>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>

using boost::asio::ip::tcp;

/**
 * @brief Класс для управления участником в комнате.
 */
class personInRoom : public std::enable_shared_from_this<personInRoom> {
public:
  personInRoom(boost::asio::io_service &io_service,
               boost::asio::io_service::strand &strand, const std::string &room)
      : io_service_(io_service), strand_(strand), room_(room),
        socket_(io_service) {}

  tcp::socket &socket() { return socket_; }

  void handleConnect(const boost::system::error_code &error) {
    if (!error) {
      std::cout << "Successfully connected" << std::endl;
    } else {
      throw std::runtime_error("Connection error");
    }
  }

  void handleRead(const boost::system::error_code &error) {
    if (!error) {
      std::cout << "Successfully read message" << std::endl;
    } else {
      throw std::runtime_error("Read error");
    }
  }

private:
  boost::asio::io_service &io_service_;
  boost::asio::io_service::strand &strand_;
  std::string room_;
  tcp::socket socket_;
};

/**
 * @brief Класс для управления сервером чата.
 */
class server {
public:
  server(boost::asio::io_service &io_service,
         boost::asio::io_service::strand &strand, const tcp::endpoint &endpoint,
         const std::string &room)
      : io_service_(io_service), strand_(strand),
        acceptor_(io_service, endpoint), room_(room) {}

  void run() { doAccept(); }

private:
  void doAccept() {
    auto new_participant =
        std::make_shared<personInRoom>(io_service_, strand_, room_);
    acceptor_.async_accept(
        new_participant->socket(),
        [this, new_participant](boost::system::error_code ec) {
          if (!ec) {
            std::cout << "New participant connected" << std::endl;
          } else {
            std::cerr << "Accept error: " << ec.message() << std::endl;
          }
          doAccept();
        });
  }

  boost::asio::io_service &io_service_;
  boost::asio::io_service::strand &strand_;
  tcp::acceptor acceptor_;
  std::string room_;
};

/**
 * @brief Тестовый кейс для проверки запуска сервера.
 */
TEST_CASE("Start server") {
  boost::asio::io_service io_service;
  boost::asio::io_service::strand strand(io_service);
  std::string room = "history.txt";

  tcp::endpoint endpoint(tcp::v4(), 12345);
  auto s = std::make_shared<server>(io_service, strand, endpoint, room);

  // Положительный тест: проверка успешного запуска сервера
  CHECK_NOTHROW(s->run());

  // Отрицательный тест: проверка запуска сервера с неинициализированным
  // io_service
  boost::asio::io_service *null_io_service = nullptr;
  CHECK_THROWS(
      std::make_shared<server>(*null_io_service, strand, endpoint, room)
          ->run());
}

/**
 * @brief Тестовый кейс для проверки обработки подключения участников.
 */
TEST_CASE("Handle connection") {
  boost::asio::io_service io_service;
  boost::asio::io_service::strand strand(io_service);
  std::string room = "history.txt";
  auto participant = std::make_shared<personInRoom>(io_service, strand, room);

  // Положительный тест: проверка успешного подключения участника
  boost::system::error_code ec;
  CHECK_NOTHROW(participant->handleConnect(ec));

  // Отрицательный тест: проверка обработки ошибки подключения участника
  ec = boost::asio::error::host_not_found;
  CHECK_THROWS(participant->handleConnect(ec));
}

/**
 * @brief Тестовый кейс для проверки обработки чтения сообщений участником.
 */
TEST_CASE("Handle read") {
  boost::asio::io_service io_service;
  boost::asio::io_service::strand strand(io_service);
  std::string room = "history.txt";
  auto participant = std::make_shared<personInRoom>(io_service, strand, room);

  // Положительный тест: проверка успешного чтения сообщения участником
  boost::system::error_code ec;
  CHECK_NOTHROW(participant->handleRead(ec));

  // Отрицательный тест: проверка обработки ошибки чтения сообщения участником
  ec = boost::asio::error::eof;
  CHECK_THROWS(participant->handleRead(ec));
}
