#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <../external/doctest/doctest.h>
#include <boost/asio.hpp>
#include <deque>
#include <iostream>
#include <string>
#include <thread>

// Параметры протокола
const int MAX_NICKNAME = 30;
const int MAX_IP_PACK_SIZE = 512;

// Класс chatClient
class chatClient : public std::enable_shared_from_this<chatClient> {
public:
  chatClient(boost::asio::io_service &io_service,
             boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
             const std::string &nickname)
      : io_service_(io_service), socket_(io_service) {
    std::strncpy(nickname_.data(), nickname.c_str(), nickname_.size() - 1);
    nickname_[nickname_.size() - 1] = '\0';
    testConnect(endpoint_iterator); // Изменение здесь
  }

  void write(const std::string &msg) {
    io_service_.post([this, msg]() { doWrite(msg); });
  }

  void close() {
    io_service_.post([this]() { socket_.close(); });
  }

  void testConnect(boost::asio::ip::tcp::resolver::iterator
                       endpoint_iterator) { // Публичный метод для тестирования
    doConnect(endpoint_iterator);
  }

private:
  void doConnect(boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {
    boost::asio::async_connect(
        socket_, endpoint_iterator,
        [this](boost::system::error_code ec,
               boost::asio::ip::tcp::resolver::iterator) {
          if (!ec) {
            doWrite(nickname_.data());
          }
        });
  }

  void doWrite(const std::string &msg) {
    bool write_in_progress = !write_msgs_.empty();
    std::array<char, MAX_IP_PACK_SIZE> formatted_msg;
    std::strncpy(formatted_msg.data(), msg.c_str(), formatted_msg.size() - 1);
    formatted_msg[formatted_msg.size() - 1] = '\0';
    write_msgs_.push_back(formatted_msg);
    if (!write_in_progress) {
      boost::asio::async_write(
          socket_,
          boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
          [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
              write_msgs_.pop_front();
              if (!write_msgs_.empty()) {
                doWrite(write_msgs_.front().data());
              }
            } else {
              socket_.close();
            }
          });
    }
  }

  void doRead() {
    boost::asio::async_read(
        socket_, boost::asio::buffer(read_msg_, read_msg_.size()),
        [this](boost::system::error_code ec, std::size_t) {
          if (!ec) {
            std::cout.write(read_msg_.data(), read_msg_.size());
            doRead();
          } else {
            socket_.close();
          }
        });
  }

  boost::asio::io_service &io_service_;
  boost::asio::ip::tcp::socket socket_;
  std::array<char, MAX_NICKNAME> nickname_;
  std::array<char, MAX_IP_PACK_SIZE> read_msg_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_msgs_;
};

// Тесты
TEST_CASE("Создание клиента") {
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query("localhost", "12345");
  boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

  std::string nickname = "test_user";

  SUBCASE("Положительный тест") {
    CHECK_NOTHROW(chatClient client(io_service, iterator, nickname));
  }

  SUBCASE("Отрицательный тест") {
    try {
      chatClient client(io_service, iterator, "");
      FAIL("Expected exception not thrown");
    } catch (const std::exception &e) {
      CHECK(std::string(e.what()).find("Expected exception message") !=
            std::string::npos);
    } catch (...) {
      FAIL("Unexpected exception type thrown");
    }
  }
}

TEST_CASE("Запись и закрытие клиента") {
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query("localhost", "12345");
  boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

  std::string nickname = "test_user";
  auto client = std::make_shared<chatClient>(io_service, iterator, nickname);

  SUBCASE("Положительный тест") {
    CHECK_NOTHROW(client->write("Hello, World!"));
    CHECK_NOTHROW(client->close());
  }

  SUBCASE("Отрицательный тест") { CHECK_THROWS(client->write("")); }
}

TEST_CASE("Подключение клиента") {
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query("localhost", "12345");
  boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

  std::string nickname = "test_user";
  auto client = std::make_shared<chatClient>(io_service, iterator, nickname);

  SUBCASE("Положительный тест") {
    CHECK_NOTHROW(client->testConnect(iterator));
  }

  SUBCASE("Отрицательный тест") {
    boost::asio::ip::tcp::resolver::query bad_query("invalid_host", "12345");
    boost::asio::ip::tcp::resolver::iterator bad_iterator =
        resolver.resolve(bad_query);
    CHECK_THROWS(client->testConnect(bad_iterator));
  }
}
