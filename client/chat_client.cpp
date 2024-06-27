#include "protocol.hpp"
#include <array>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <thread>

using boost::asio::ip::tcp;

/**
 * @class client
 * @brief Класс клиента для подключения к серверу чата и общения с другими
 * клиентами.
 */
class client {
public:
  /**
   * @brief Конструктор клиента.
   * @param nickname Никнейм клиента.
   * @param io_service Сервис ввода-вывода Boost.Asio.
   * @param endpoint_iterator Итератор конечных точек для подключения к серверу.
   */
  client(const std::array<char, MAX_NICKNAME> &nickname,
         boost::asio::io_service &io_service,
         tcp::resolver::iterator endpoint_iterator)
      : io_service_(io_service), socket_(io_service) {
    strcpy(nickname_.data(), nickname.data());
    memset(read_msg_.data(), '\0', MAX_IP_PACK_SIZE);
    boost::asio::async_connect(socket_, endpoint_iterator,
                               boost::bind(&client::onConnect, this, _1));
  }

  /**
   * @brief Отправка сообщения на сервер.
   * @param msg Сообщение для отправки.
   */
  void write(const std::array<char, MAX_IP_PACK_SIZE> &msg) {
    io_service_.post(boost::bind(&client::writeImpl, this, msg));
  }

  /**
   * @brief Закрытие подключения клиента.
   */
  void close() { io_service_.post(boost::bind(&client::closeImpl, this)); }

private:
  /**
   * @brief Обработчик события подключения к серверу.
   * @param error Код ошибки.
   */
  void onConnect(const boost::system::error_code &error) {
    if (!error) {
      boost::asio::async_write(socket_,
                               boost::asio::buffer(nickname_, nickname_.size()),
                               boost::bind(&client::readHandler, this, _1));
    }
  }

  /**
   * @brief Обработчик чтения сообщения от сервера.
   * @param error Код ошибки.
   */
  void readHandler(const boost::system::error_code &error) {
    std::cout << read_msg_.data() << std::endl;
    if (!error) {
      boost::asio::async_read(socket_,
                              boost::asio::buffer(read_msg_, read_msg_.size()),
                              boost::bind(&client::readHandler, this, _1));
    } else {
      closeImpl();
    }
  }

  /**
   * @brief Реализация отправки сообщения.
   * @param msg Сообщение для отправки.
   */
  void writeImpl(std::array<char, MAX_IP_PACK_SIZE> msg) {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
      boost::asio::async_write(
          socket_,
          boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
          boost::bind(&client::writeHandler, this, _1));
    }
  }

  /**
   * @brief Обработчик события завершения отправки сообщения.
   * @param error Код ошибки.
   */
  void writeHandler(const boost::system::error_code &error) {
    if (!error) {
      write_msgs_.pop_front();
      if (!write_msgs_.empty()) {
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(write_msgs_.front(),
                                write_msgs_.front().size()),
            boost::bind(&client::writeHandler, this, _1));
      }
    } else {
      closeImpl();
    }
  }

  /**
   * @brief Реализация закрытия подключения.
   */
  void closeImpl() { socket_.close(); }

  boost::asio::io_service &io_service_;
  tcp::socket socket_;
  std::array<char, MAX_IP_PACK_SIZE> read_msg_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_msgs_;
  std::array<char, MAX_NICKNAME> nickname_;
};

int main(int argc, char *argv[]) {
  try {
    if (argc != 4) {
      std::cerr << "Использование: chat_client <nickname> <host> <port>\n";
      return 1;
    }
    boost::asio::io_service io_service;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(argv[2], argv[3]);
    tcp::resolver::iterator iterator = resolver.resolve(query);
    std::array<char, MAX_NICKNAME> nickname;
    strcpy(nickname.data(), argv[1]);

    client cli(nickname, io_service, iterator);

    std::thread t(boost::bind(&boost::asio::io_service::run, &io_service));

    std::array<char, MAX_IP_PACK_SIZE> msg;

    while (true) {
      memset(msg.data(), '\0', msg.size());
      if (!std::cin.getline(msg.data(),
                            MAX_IP_PACK_SIZE - PADDING - MAX_NICKNAME)) {
        std::cin.clear(); // очистка ошибки и продолжение чтения
      }
      cli.write(msg);
    }

    cli.close();
    t.join();
  } catch (std::exception &e) {
    std::cerr << "Исключение: " << e.what() << "\n";
  }

  return 0;
}
