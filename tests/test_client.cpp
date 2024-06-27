#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../client/client.hpp"
#include <../external/doctest/doctest.h>
#include <boost/asio.hpp>

TEST_CASE("Создание клиента") {
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query("localhost", "12345");
  boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

  std::array<char, MAX_NICKNAME> nickname = {'t', 'e', 's', 't', '_',
                                             'u', 's', 'e', 'r', '\0'};

  SUBCASE("Положительный тест") {
    CHECK_NOTHROW(client cli(nickname, io_service, iterator));
  }

  SUBCASE("Отрицательный тест: пустой никнейм") {
    std::array<char, MAX_NICKNAME> empty_nickname = {'\0'};
    CHECK_THROWS_AS(
        [&]() { client cli(empty_nickname, io_service, iterator); }(),
        std::runtime_error);
  }
}

TEST_CASE("Отправка сообщений и закрытие клиента") {
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query("localhost", "12345");
  boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

  std::array<char, MAX_NICKNAME> nickname = {'t', 'e', 's', 't', '_',
                                             'u', 's', 'e', 'r', '\0'};
  client cli(nickname, io_service, iterator);

  SUBCASE("Положительный тест") {
    std::array<char, MAX_IP_PACK_SIZE> msg = {'H', 'e', 'l', 'l', 'o', '\0'};
    CHECK_NOTHROW(cli.write(msg));
    CHECK_NOTHROW(cli.close());
  }

  SUBCASE("Отрицательный тест: пустое сообщение") {
    std::array<char, MAX_IP_PACK_SIZE> empty_msg = {'\0'};
    CHECK_NOTHROW(cli.write(empty_msg)); // пустое соо не должно вызывать ошибки
    CHECK_NOTHROW(cli.close());
  }
}

TEST_CASE("Подключение клиента") {
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query("localhost", "12345");
  boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

  std::array<char, MAX_NICKNAME> nickname = {'t', 'e', 's', 't', '_',
                                             'u', 's', 'e', 'r', '\0'};
  client cli(nickname, io_service, iterator);

  SUBCASE("Положительный тест") {
    boost::system::error_code ec;
    CHECK_NOTHROW(cli.onConnect(ec));
  }

  SUBCASE("Отрицательный тест: ошибка подключения") {
    boost::system::error_code ec = boost::asio::error::host_not_found;
    CHECK_THROWS_AS(cli.onConnect(ec), std::runtime_error);
  }
}
