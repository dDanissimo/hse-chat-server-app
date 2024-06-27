#ifndef SERVER_HPP
#define SERVER_HPP

#include <array>
#include <boost/asio.hpp>
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

constexpr int MAX_NICKNAME = 16;
constexpr int MAX_IP_PACK_SIZE = 512;

using boost::asio::ip::tcp;

/**
 * @class participant
 * @brief Абстрактный класс для участников чата.
 */
class participant {
public:
  virtual ~participant() {}
  virtual void onMessage(std::array<char, MAX_IP_PACK_SIZE> &msg) = 0;
};

/**
 * @class chatRoom
 * @brief Класс для управления комнатой чата.
 */
class chatRoom {
public:
  chatRoom();

  /**
   * @brief Участник заходит в комнату.
   * @param participant Указатель на участника.
   * @param nickname Никнейм участника.
   */
  void enter(std::shared_ptr<participant> participant,
             const std::string &nickname);

  /**
   * @brief Участник выходит из комнаты.
   * @param participant Указатель на участника.
   */
  void leave(std::shared_ptr<participant> participant);

  /**
   * @brief Отправка сообщения всем участникам.
   * @param msg Сообщение для отправки.
   * @param participant Указатель на участника, отправившего сообщение.
   */
  void broadcast(std::array<char, MAX_IP_PACK_SIZE> &msg,
                 std::shared_ptr<participant> participant);

  /**
   * @brief Получение никнейма участника.
   * @param participant Указатель на участника.
   * @return Никнейм участника.
   */
  std::string getNickname(std::shared_ptr<participant> participant);

private:
  /**
   * @brief Сохранение сообщения в файл.
   * @param msg Сообщение для сохранения.
   */
  void saveMessage(const std::array<char, MAX_IP_PACK_SIZE> &msg);

  /**
   * @brief Загрузка истории сообщений из файла.
   */
  void loadHistory();

  std::unordered_set<std::shared_ptr<participant>> participants_;
  std::unordered_map<std::shared_ptr<participant>, std::string> name_table_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> recent_msgs_;
  enum { max_recent_msgs = 100 };
};

/**
 * @class personInRoom
 * @brief Класс для управления участником в комнате.
 */
class personInRoom : public participant,
                     public std::enable_shared_from_this<personInRoom> {
public:
  /**
   * @brief Конструктор.
   * @param io_service Сервис ввода-вывода Boost.Asio.
   * @param strand Странд Boost.Asio.
   * @param room Комната чата.
   */
  personInRoom(boost::asio::io_service &io_service,
               boost::asio::io_service::strand &strand, chatRoom &room);
  /**
   * @brief Получение сокета.
   * @return Сокет.
   */
  tcp::socket &socket();
  /**
   * @brief Запуск участника.
   */
  void start();
  /**
   * @brief Обработка входящего сообщения.
   * @param msg Сообщение.
   */
  void onMessage(std::array<char, MAX_IP_PACK_SIZE> &msg);
  /**
   * @brief Обработчик никнейма.
   * @param error Код ошибки.
   */
  void nicknameHandler(const boost::system::error_code &error);
  /**
   * @brief Обработчик чтения.
   * @param error Код ошибки.
   */
  void readHandler(const boost::system::error_code &error);

private:
  /**
   * @brief Обработчик записи.
   * @param error Код ошибки.
   */
  void writeHandler(const boost::system::error_code &error);

  tcp::socket socket_;
  boost::asio::io_service::strand &strand_;
  chatRoom &room_;
  std::array<char, MAX_NICKNAME> nickname_;
  std::array<char, MAX_IP_PACK_SIZE> read_msg_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_msgs_;
};

/**
 * @class server
 * @brief Класс сервера чата.
 */
class server {
public:
  /**
   * @brief Конструктор сервера.
   * @param io_service Сервис ввода-вывода Boost.Asio.
   * @param strand Странд Boost.Asio.
   * @param endpoint Конечная точка подключения.
   */
  server(boost::asio::io_service &io_service,
         boost::asio::io_service::strand &strand,
         const tcp::endpoint &endpoint);

private:
  /**
   * @brief Запуск сервера.
   */
  void run();

  /**
   * @brief Обработчик подключения нового участника.
   * @param new_participant Новый участник.
   * @param error Код ошибки.
   */
  void onAccept(std::shared_ptr<personInRoom> new_participant,
                const boost::system::error_code &error);

  boost::asio::io_service &io_service_;
  boost::asio::io_service::strand &strand_;
  tcp::acceptor acceptor_;
  chatRoom room_;
};
/**
 * @brief Загружает конфигурацию из файла и парсит её в объект JSON.
 *
 * @param filename Имя файла конфигурации.
 * @param config Объект nlohmann::json, в который будет загружена конфигурация.
 *
 * @throws std::runtime_error Если файл конфигурации не может быть открыт.
 */
void loadConfig(const std::string &filename, nlohmann::json &config);

#endif // SERVER_HPP
