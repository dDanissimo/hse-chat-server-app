#include "protocol.hpp"
#include <algorithm>
#include <array>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread/thread.hpp>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using boost::asio::ip::tcp;
using namespace boost::placeholders;

namespace {

/**
 * @brief Получение текущего временного штампа.
 * @return Временной штамп в виде строки.
 */
std::string getTimestamp() {
  time_t t = time(0);
  struct tm *now = localtime(&t);
  std::stringstream ss;
  ss << '[' << (now->tm_year + 1900) << '-' << std::setfill('0') << std::setw(2)
     << (now->tm_mon + 1) << '-' << std::setfill('0') << std::setw(2)
     << now->tm_mday << ' ' << std::setfill('0') << std::setw(2) << now->tm_hour
     << ":" << std::setfill('0') << std::setw(2) << now->tm_min << ":"
     << std::setfill('0') << std::setw(2) << now->tm_sec << "] ";
  return ss.str();
}

std::mutex log_mutex;

/**
 * @brief Логирование сообщения в файл.
 * @param message Сообщение для логирования.
 */
void log(const std::string &message) {
  std::lock_guard<std::mutex> lock(log_mutex);
  std::ofstream log_file("server.log", std::ios::app);
  if (log_file.is_open()) {
    log_file << getTimestamp() << message << std::endl;
  }
}

class workerThread {
public:
  /**
   * @brief Запуск рабочей нити.
   * @param io_service Сервис ввода-вывода Boost.Asio.
   */
  static void run(std::shared_ptr<boost::asio::io_service> io_service) {
    {
      std::lock_guard<std::mutex> lock(m);
      std::cout << "[" << std::this_thread::get_id() << "] Нить запущена"
                << std::endl;
    }

    io_service->run();

    {
      std::lock_guard<std::mutex> lock(m);
      std::cout << "[" << std::this_thread::get_id() << "] Нить завершена"
                << std::endl;
    }
  }

private:
  static std::mutex m;
};

std::mutex workerThread::m;

} // namespace

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
  /**
   * @brief Конструктор комнаты чата.
   * Загружает историю сообщений из файла.
   */
  chatRoom() { loadHistory(); }

  /**
   * @brief Участник заходит в комнату.
   * @param participant Указатель на участника.
   * @param nickname Никнейм участника.
   */
  void enter(std::shared_ptr<participant> participant,
             const std::string &nickname) {
    participants_.insert(participant);
    name_table_[participant] = nickname;
    std::for_each(recent_msgs_.begin(), recent_msgs_.end(),
                  boost::bind(&participant::onMessage, participant, _1));
    log("Пользователь " + nickname + " вошел в комнату.");
  }

  /**
   * @brief Участник выходит из комнаты.
   * @param participant Указатель на участника.
   */
  void leave(std::shared_ptr<participant> participant) {
    std::string nickname = name_table_[participant];
    participants_.erase(participant);
    name_table_.erase(participant);
    log("Пользователь " + nickname + " вышел из комнаты.");
  }

  /**
   * @brief Отправка сообщения всем участникам.
   * @param msg Сообщение для отправки.
   * @param participant Указатель на участника, отправившего сообщение.
   */
  void broadcast(std::array<char, MAX_IP_PACK_SIZE> &msg,
                 std::shared_ptr<participant> participant) {
    std::string timestamp = getTimestamp();
    std::string nickname = getNickname(participant);
    std::array<char, MAX_IP_PACK_SIZE> formatted_msg;

    strcpy(formatted_msg.data(), timestamp.c_str());
    strcat(formatted_msg.data(), nickname.c_str());
    strcat(formatted_msg.data(), msg.data());

    recent_msgs_.push_back(formatted_msg);
    while (recent_msgs_.size() > max_recent_msgs) {
      recent_msgs_.pop_front();
    }

    log("Сообщение от " + nickname + ": " + msg.data());
    saveMessage(formatted_msg);

    std::for_each(
        participants_.begin(), participants_.end(),
        boost::bind(&participant::onMessage, _1, std::ref(formatted_msg)));
  }

  /**
   * @brief Получение никнейма участника.
   * @param participant Указатель на участника.
   * @return Никнейм участника.
   */
  std::string getNickname(std::shared_ptr<participant> participant) {
    return name_table_[participant];
  }

private:
  /**
   * @brief Сохранение сообщения в файл.
   * @param msg Сообщение для сохранения.
   */
  void saveMessage(const std::array<char, MAX_IP_PACK_SIZE> &msg) {
    std::ofstream file("chat_history.txt", std::ios::app);
    if (file.is_open()) {
      file << msg.data() << std::endl;
    }
  }

  /**
   * @brief Загрузка истории сообщений из файла.
   */
  void loadHistory() {
    std::ifstream file("chat_history.txt");
    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        std::array<char, MAX_IP_PACK_SIZE> msg;
        std::fill(msg.begin(), msg.end(), 0);
        std::copy(line.begin(), line.end(), msg.begin());
        recent_msgs_.push_back(msg);
      }
    }
  }

  /**
   * @brief Получение текущего временного штампа.
   * @return Временной штамп в виде строки.
   */
  std::string getTimestamp() {
    time_t t = time(0);
    struct tm *now = localtime(&t);
    std::stringstream ss;
    ss << '[' << (now->tm_year + 1900) << '-' << std::setfill('0')
       << std::setw(2) << (now->tm_mon + 1) << '-' << std::setfill('0')
       << std::setw(2) << now->tm_mday << ' ' << std::setfill('0')
       << std::setw(2) << now->tm_hour << ":" << std::setfill('0')
       << std::setw(2) << now->tm_min << ":" << std::setfill('0')
       << std::setw(2) << now->tm_sec << "] ";

    return ss.str();
  }

  enum { max_recent_msgs = 100 };
  std::unordered_set<std::shared_ptr<participant>> participants_;
  std::unordered_map<std::shared_ptr<participant>, std::string> name_table_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> recent_msgs_;
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
               boost::asio::io_service::strand &strand, chatRoom &room)
      : socket_(io_service), strand_(strand), room_(room) {}

  /**
   * @brief Получение сокета.
   * @return Сокет.
   */
  tcp::socket &socket() { return socket_; }

  /**
   * @brief Запуск участника.
   */
  void start() {
    boost::asio::async_read(
        socket_, boost::asio::buffer(nickname_, nickname_.size()),
        strand_.wrap(boost::bind(&personInRoom::nicknameHandler,
                                 shared_from_this(), _1)));
  }

  /**
   * @brief Обработка входящего сообщения.
   * @param msg Сообщение.
   */
  void onMessage(std::array<char, MAX_IP_PACK_SIZE> &msg) {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
      boost::asio::async_write(
          socket_,
          boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
          strand_.wrap(boost::bind(&personInRoom::writeHandler,
                                   shared_from_this(), _1)));
    }
  }

private:
  /**
   * @brief Обработчик никнейма.
   * @param error Код ошибки.
   */
  void nicknameHandler(const boost::system::error_code &error) {
    if (strlen(nickname_.data()) <= MAX_NICKNAME - 2) {
      strcat(nickname_.data(), ": ");
    } else {
      nickname_[MAX_NICKNAME - 2] = ':';
      nickname_[MAX_NICKNAME - 1] = ' ';
    }

    room_.enter(shared_from_this(), std::string(nickname_.data()));

    boost::asio::async_read(socket_,
                            boost::asio::buffer(read_msg_, read_msg_.size()),
                            strand_.wrap(boost::bind(&personInRoom::readHandler,
                                                     shared_from_this(), _1)));
  }

  /**
   * @brief Обработчик чтения.
   * @param error Код ошибки.
   */
  void readHandler(const boost::system::error_code &error) {
    if (!error) {
      room_.broadcast(read_msg_, shared_from_this());

      boost::asio::async_read(
          socket_, boost::asio::buffer(read_msg_, read_msg_.size()),
          strand_.wrap(
              boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
    } else {
      room_.leave(shared_from_this());
    }
  }

  /**
   * @brief Обработчик записи.
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
            strand_.wrap(boost::bind(&personInRoom::writeHandler,
                                     shared_from_this(), _1)));
      }
    } else {
      room_.leave(shared_from_this());
    }
  }

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
         boost::asio::io_service::strand &strand, const tcp::endpoint &endpoint)
      : io_service_(io_service), strand_(strand),
        acceptor_(io_service, endpoint) {
    run();
  }

private:
  /**
   * @brief Запуск сервера.
   */
  void run() {
    std::shared_ptr<personInRoom> new_participant(
        new personInRoom(io_service_, strand_, room_));
    acceptor_.async_accept(new_participant->socket(),
                           strand_.wrap(boost::bind(&server::onAccept, this,
                                                    new_participant, _1)));
  }

  /**
   * @brief Обработчик подключения нового участника.
   * @param new_participant Новый участник.
   * @param error Код ошибки.
   */
  void onAccept(std::shared_ptr<personInRoom> new_participant,
                const boost::system::error_code &error) {
    if (!error) {
      new_participant->start();
      log("Подключение нового участника");
    }

    run();
  }

  boost::asio::io_service &io_service_;
  boost::asio::io_service::strand &strand_;
  tcp::acceptor acceptor_;
  chatRoom room_;
};

/**
 * @brief Точка входа в программу сервера.
 * @param argc Количество аргументов командной строки.
 * @param argv Аргументы командной строки.
 * @return Код завершения программы.
 */
int main(int argc, char *argv[]) {
  try {
    std::ifstream config_file("config/config.json");
    if (!config_file.is_open()) {
      std::cerr << "Не удалось открыть файл конфигурации.\n";
      return 1;
    }

    nlohmann::json config;
    config_file >> config;
    std::vector<int> ports = config["ports"];

    if (ports.empty()) {
      std::cerr << "Нет указанных портов в конфигурационном файле.\n";
      return 1;
    }

    std::shared_ptr<boost::asio::io_service> io_service(
        new boost::asio::io_service);
    boost::shared_ptr<boost::asio::io_service::work> work(
        new boost::asio::io_service::work(*io_service));
    boost::shared_ptr<boost::asio::io_service::strand> strand(
        new boost::asio::io_service::strand(*io_service));

    std::cout << "[" << std::this_thread::get_id() << "] Сервер запущен"
              << std::endl;
    log("Сервер запущен");

    std::list<std::shared_ptr<server>> servers;
    for (int port : ports) {
      tcp::endpoint endpoint(tcp::v4(), port);
      std::shared_ptr<server> a_server(
          new server(*io_service, *strand, endpoint));
      servers.push_back(a_server);
    }

    boost::thread_group workers;
    for (int i = 0; i < 1; ++i) {
      boost::thread *t =
          new boost::thread{boost::bind(&workerThread::run, io_service)};

#ifdef __linux__
      // привязка процессора для рабочей нити в Linux
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(i, &cpuset);
      pthread_setaffinity_np(t->native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
      workers.add_thread(t);
    }

    workers.join_all();
  } catch (std::exception &e) {
    std::cerr << "Исключение: " << e.what() << "\n";
    log(std::string("Исключение: ") + e.what());
  }

  return 0;
}
