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

class participant {
public:
  virtual ~participant() {}
  virtual void onMessage(std::array<char, MAX_IP_PACK_SIZE> &msg) = 0;
};

class chatRoom {
public:
  chatRoom();
  void enter(std::shared_ptr<participant> participant,
             const std::string &nickname);
  void leave(std::shared_ptr<participant> participant);
  void broadcast(std::array<char, MAX_IP_PACK_SIZE> &msg,
                 std::shared_ptr<participant> participant);
  std::string getNickname(std::shared_ptr<participant> participant);

private:
  void saveMessage(const std::array<char, MAX_IP_PACK_SIZE> &msg);
  void loadHistory();

  std::unordered_set<std::shared_ptr<participant>> participants_;
  std::unordered_map<std::shared_ptr<participant>, std::string> name_table_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> recent_msgs_;
  enum { max_recent_msgs = 100 };
};

class personInRoom : public participant,
                     public std::enable_shared_from_this<personInRoom> {
public:
  personInRoom(boost::asio::io_service &io_service,
               boost::asio::io_service::strand &strand, chatRoom &room);

  tcp::socket &socket();
  void start();
  void onMessage(std::array<char, MAX_IP_PACK_SIZE> &msg);
  void nicknameHandler(const boost::system::error_code &error);
  void readHandler(const boost::system::error_code &error);

private:
  void writeHandler(const boost::system::error_code &error);

  tcp::socket socket_;
  boost::asio::io_service::strand &strand_;
  chatRoom &room_;
  std::array<char, MAX_NICKNAME> nickname_;
  std::array<char, MAX_IP_PACK_SIZE> read_msg_;
  std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_msgs_;
};

class server {
public:
  server(boost::asio::io_service &io_service,
         boost::asio::io_service::strand &strand,
         const tcp::endpoint &endpoint);

private:
  void run();
  void onAccept(std::shared_ptr<personInRoom> new_participant,
                const boost::system::error_code &error);

  boost::asio::io_service &io_service_;
  boost::asio::io_service::strand &strand_;
  tcp::acceptor acceptor_;
  chatRoom room_;
};

void loadConfig(const std::string &filename, nlohmann::json &config);

#endif // SERVER_HPP
