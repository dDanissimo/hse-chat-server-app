#include "server.hpp"
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/thread.hpp>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <thread>

using namespace boost::placeholders;

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

void log(const std::string &message) {
  std::ofstream log_file("server.log", std::ios::app);
  if (log_file.is_open()) {
    log_file << getTimestamp() << message << std::endl;
  }
}

void loadConfig(const std::string &filename, nlohmann::json &config) {
  std::ifstream config_file(filename);
  if (!config_file.is_open()) {
    throw std::runtime_error("Не удалось открыть файл конфигурации.");
  }
  config_file >> config;
}

chatRoom::chatRoom() { loadHistory(); }

void chatRoom::enter(std::shared_ptr<participant> participant,
                     const std::string &nickname) {
  participants_.insert(participant);
  name_table_[participant] = nickname;
  for (auto &msg : recent_msgs_) {
    participant->onMessage(msg);
  }
  log("Пользователь " + nickname + " вошел в комнату.");
}

void chatRoom::leave(std::shared_ptr<participant> participant) {
  std::string nickname = name_table_[participant];
  participants_.erase(participant);
  name_table_.erase(participant);
  log("Пользователь " + nickname + " вышел из комнаты.");
}

void chatRoom::broadcast(std::array<char, MAX_IP_PACK_SIZE> &msg,
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

  for (auto &p : participants_) {
    p->onMessage(formatted_msg);
  }
}

std::string chatRoom::getNickname(std::shared_ptr<participant> participant) {
  return name_table_[participant];
}

void chatRoom::saveMessage(const std::array<char, MAX_IP_PACK_SIZE> &msg) {
  std::ofstream file("chat_history.txt", std::ios::app);
  if (file.is_open()) {
    file << msg.data() << std::endl;
  }
}

void chatRoom::loadHistory() {
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

personInRoom::personInRoom(boost::asio::io_service &io_service,
                           boost::asio::io_service::strand &strand,
                           chatRoom &room)
    : socket_(io_service), strand_(strand), room_(room) {}

tcp::socket &personInRoom::socket() { return socket_; }

void personInRoom::start() {
  auto self(shared_from_this());
  boost::asio::async_read(
      socket_, boost::asio::buffer(nickname_, nickname_.size()),
      strand_.wrap(boost::bind(&personInRoom::nicknameHandler, self, _1)));
}

void personInRoom::onMessage(std::array<char, MAX_IP_PACK_SIZE> &msg) {
  auto self(shared_from_this());
  bool write_in_progress = !write_msgs_.empty();
  write_msgs_.push_back(msg);
  if (!write_in_progress) {
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
        strand_.wrap(boost::bind(&personInRoom::writeHandler, self, _1)));
  }
}

void personInRoom::nicknameHandler(const boost::system::error_code &error) {
  if (error) {
    room_.leave(shared_from_this());
    log("Ошибка подключения: " + error.message());
    throw std::runtime_error("Ошибка подключения: " + error.message());
    return;
  }

  if (strlen(nickname_.data()) <= MAX_NICKNAME - 2) {
    strcat(nickname_.data(), ": ");
  } else {
    nickname_[MAX_NICKNAME - 2] = ':';
    nickname_[MAX_NICKNAME - 1] = ' ';
  }

  room_.enter(shared_from_this(), std::string(nickname_.data()));

  auto self(shared_from_this());
  boost::asio::async_read(
      socket_, boost::asio::buffer(read_msg_, read_msg_.size()),
      strand_.wrap(boost::bind(&personInRoom::readHandler, self, _1)));
}

void personInRoom::readHandler(const boost::system::error_code &error) {
  if (error) {
    if (error == boost::asio::error::eof) {
      log("Клиент закрыл соединение: " + error.message());
    } else {
      log("Ошибка чтения сообщения: " + error.message());
    }
    room_.leave(shared_from_this());
    throw std::runtime_error("Ошибка чтения сообщения: " + error.message());
    return;
  }

  room_.broadcast(read_msg_, shared_from_this());

  auto self(shared_from_this());
  boost::asio::async_read(
      socket_, boost::asio::buffer(read_msg_, read_msg_.size()),
      strand_.wrap(boost::bind(&personInRoom::readHandler, self, _1)));
}

void personInRoom::writeHandler(const boost::system::error_code &error) {
  if (error) {
    log("Ошибка записи сообщения: " + error.message());
    room_.leave(shared_from_this());
    return;
  }
  write_msgs_.pop_front();
  if (!write_msgs_.empty()) {
    auto self(shared_from_this());
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
        strand_.wrap(boost::bind(&personInRoom::writeHandler, self, _1)));
  }
}

server::server(boost::asio::io_service &io_service,
               boost::asio::io_service::strand &strand,
               const tcp::endpoint &endpoint)
    : io_service_(io_service), strand_(strand),
      acceptor_(io_service, endpoint) {
  run();
}

void server::run() {
  std::shared_ptr<personInRoom> new_participant(
      new personInRoom(io_service_, strand_, room_));
  acceptor_.async_accept(
      new_participant->socket(),
      strand_.wrap(boost::bind(&server::onAccept, this, new_participant, _1)));
}

void server::onAccept(std::shared_ptr<personInRoom> new_participant,
                      const boost::system::error_code &error) {
  if (!error) {
    new_participant->start();
    log("Подключение нового участника");
  } else {
    log("Ошибка подключения нового участника: " + error.message());
  }
  run();
}

#ifndef UNIT_TEST
int main(int argc, char *argv[]) {
  try {
    nlohmann::json config;
    loadConfig("config/config.json", config);
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
      boost::thread *t = new boost::thread{
          boost::bind(&boost::asio::io_service::run, io_service)};
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
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
#endif // UNIT_TEST
