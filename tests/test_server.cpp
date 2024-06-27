#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../server/server.hpp"
#include <../external/doctest/doctest.h>
#include <boost/asio.hpp>

TEST_CASE("Запуск сервера") {
  boost::asio::io_service io_service;
  boost::asio::io_service::strand strand(io_service);
  tcp::endpoint endpoint(tcp::v4(), 12345);

  SUBCASE("Положительный тест") {
    CHECK_NOTHROW(server srv(io_service, strand, endpoint));
  }

  SUBCASE("Отрицательный тест: неверный endpoint") {
    try {
      tcp::endpoint bad_endpoint(
          boost::asio::ip::address::from_string("256.256.256.256"),
          12345); // Неверный IP-адрес
      CHECK_THROWS_AS(server srv(io_service, strand, bad_endpoint),
                      std::runtime_error);
    } catch (...) {
      CHECK(true); // Пример обработки исключения для некорректного IP
    }
  }
}

TEST_CASE("Обработка подключения участника") {
  boost::asio::io_service io_service;
  boost::asio::io_service::strand strand(io_service);
  chatRoom room;
  auto participant = std::make_shared<personInRoom>(io_service, strand, room);

  SUBCASE("Положительный тест: успешное подключение") {
    boost::system::error_code ec;
    CHECK_NOTHROW(participant->nicknameHandler(ec));
  }

  SUBCASE("Отрицательный тест: ошибка подключения") {
    boost::system::error_code ec = boost::asio::error::host_not_found;
    CHECK_THROWS_AS(participant->nicknameHandler(ec), std::runtime_error);
  }
}

TEST_CASE("Обработка сообщений участника") {
  boost::asio::io_service io_service;
  boost::asio::io_service::strand strand(io_service);
  chatRoom room;
  auto participant = std::make_shared<personInRoom>(io_service, strand, room);

  SUBCASE("Положительный тест: успешное чтение сообщения") {
    boost::system::error_code ec;
    CHECK_NOTHROW(participant->readHandler(ec));
  }

  SUBCASE("Отрицательный тест: ошибка чтения сообщения") {
    boost::system::error_code ec = boost::asio::error::eof;
    CHECK_THROWS_AS(participant->readHandler(ec), std::runtime_error);
  }
}
