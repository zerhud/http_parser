#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE server

#include <boost/test/unit_test.hpp>
#include <http_parser.hpp>
#include <http_parser/server.hpp>

using namespace std::literals;


BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(server)

using server_t = http_parser::server<>;

// add remove reject sockets (socket owner is above abstraction)
// parse (and stop parsing) and route data
// head prepare, long body, error workout
// response or reject in handler (stop parsing from handler)
// routing:
//		full match
//		partly match
//		match with parameters (any match method)

struct tcp_socket : http_parser::tcp_socket {
	std::size_t reject_count = 0;
	void reject() override { ++reject_count; }
};

BOOST_AUTO_TEST_CASE(simple)
{
	tcp_socket sock1, sock2;
	server_t srv;
	std::size_t hndl_count=0, hndl_path_count=0;
	srv.reg("GET", "/path", [&](const server_t::message_t& head, server_t::data_view body){
		BOOST_TEST(head.head().url().path() == "/path"sv);
		BOOST_TEST(body == "123"sv);
		++hndl_path_count;
	});
	srv.reg("GET", "/", [&](const auto& head, auto body){
		BOOST_TEST(head.head().url() == "/"sv);
		BOOST_TEST(body == "test"sv);
		++hndl_count;
	});
	BOOST_TEST(hndl_count == 0);
	BOOST_TEST(hndl_path_count == 0);
	srv(&sock1, "GET / HTTP/1.1\r\nContent-Length:4\r\n\r\ntest"sv);
	BOOST_TEST(hndl_count == 1);
	BOOST_TEST(hndl_path_count == 0);
	srv(&sock2, "GET /path HTTP/1.1\r\n"sv);
	srv(&sock2, "Content-Length:3\r\n\r\n123"sv);
	BOOST_TEST(hndl_count == 1);
	BOOST_TEST(hndl_path_count == 1);

	BOOST_TEST_CONTEXT("/path with params (match only path)") {
		std::cout << "here" << std::endl;
		srv(&sock2, "GET /path?a=b HTTP/1.1\r\n"sv);
		srv(&sock2, "Content-Length:3\r\n\r\n123"sv);
		BOOST_TEST(hndl_count == 1);
		BOOST_TEST(hndl_path_count == 2);
	}
}
BOOST_AUTO_TEST_CASE(chunked)
{
	tcp_socket sock;
	server_t srv;
	std::size_t hndl_count = 0;
	srv.reg("GET", "/path", [&](const server_t::message_t& head, server_t::data_view body) {
		BOOST_TEST(body == std::string_view{std::to_string(hndl_count)});
		++hndl_count;
	});
	BOOST_TEST_CONTEXT("0") srv(&sock, "GET /path HTTP/1.1\r\nTransfer-Encoding:chunked\r\n\r\n1\r\n0"sv);
	BOOST_TEST_CONTEXT("1") srv(&sock, "1\r\n1"sv);
	BOOST_TEST_CONTEXT("2") srv(&sock, "1\r\n2"sv);
	BOOST_TEST_CONTEXT("3") srv(&sock, "1\r\n3"sv);
	BOOST_TEST_CONTEXT("4") srv(&sock, "1\r\n4"sv);
	BOOST_TEST_CONTEXT("5") srv(&sock, "1\r\n5"sv);
	BOOST_TEST_CONTEXT("6") srv(&sock, "1\r\n6"sv);
	BOOST_TEST_CONTEXT("7") srv(&sock, "1\r\n7"sv);
	BOOST_TEST_CONTEXT("8") srv(&sock, "1\r\n8"sv);
	BOOST_TEST_CONTEXT("8") srv(&sock, "1\r\n9"sv);
	BOOST_TEST_CONTEXT("10") srv(&sock, "2\r\n10"sv);
	BOOST_TEST(hndl_count == 11);
}
BOOST_AUTO_TEST_CASE(reject_sockets)
{
	tcp_socket sock;
	server_t srv;
	srv(&sock, "GET / HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(sock.reject_count == 1);
	BOOST_TEST(srv.open_sockets() == 1);
}
BOOST_AUTO_TEST_CASE(remove_sockets)
{
	tcp_socket sock1, sock2;
	server_t srv;
	BOOST_TEST(srv.open_sockets() == 0);
	srv(&sock1, "GET / HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(srv.open_sockets() == 1);
	srv.close_socket(&sock1);
	BOOST_TEST(srv.open_sockets() == 0);
	srv(&sock2, "GET / HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(srv.open_sockets() == 1);
}
BOOST_AUTO_TEST_SUITE_END() // server
BOOST_AUTO_TEST_SUITE_END() // core
