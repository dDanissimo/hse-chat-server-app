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
         tcp::resolver::iterator endpoint_iterator);
  /**
   * @brief Отправка сообщения на сервер.
   * @param msg Сообщение для отправки.
   */
  void write(const std::array<char, MAX_IP_PACK_SIZE> &msg);
  /**
   * @brief Закрытие подключения клиента.
   */
  void close();
  /**
   * @brief Обработчик события подключения к серверу.
   * @param error Код ошибки.
   */
  void onConnect(const boost::system::error_code &error);

private:
  /**
   * @brief Обработчик чтения сообщения от сервера.
   * @param error Код ошибки.
   */
  void readHandler(const boost::system::error_code &error);
  /**
   * @brief Реализация отправки сообщения.
   * @param msg Сообщение для отправки.
   */
  void writeImpl(std::array<char, MAX_IP_PACK_SIZE> msg);
  /**
   * @brief Обработчик события завершения отправки сообщения.
   * @param error Код ошибки.
   */
  void writeHandler(const boost::system::error_code &error);
  /**
   * @brief Реализация закрытия подключения.
   */
  void closeImpl();

  boost::asio::io_service &io_service_;
  tcp::socket socket_;
  std::array<char, MAX_IP_PACK_SIZE> read_msg_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_msgs_;
  std::array<char, MAX_NICKNAME> nickname_;
};

#endif // CLIENT_HPP
