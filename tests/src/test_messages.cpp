#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE messages

#include <memory_resource>
#include <boost/test/unit_test.hpp>
#include <http_parser/message.hpp>

using namespace std::literals;
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_SUITE(utils)
BOOST_AUTO_TEST_SUITE(messages)
using http_parser::header_message;
using http_parser::req_head_message;
BOOST_AUTO_TEST_SUITE(req_head)
BOOST_AUTO_TEST_CASE(empty)
{
	std::string data = "GET /path/to/resource?a=b HTTP/1.1\r\n"s;
	req_head_message msg(&data);
	BOOST_TEST(msg.url().path() == "/"sv);
	BOOST_TEST(msg.method() == ""sv);
}
BOOST_AUTO_TEST_CASE(url)
{
	std::string data = "GET /path/to/resource?a=b HTTP/1.1\r\n"s;
	req_head_message msg(&data);
	msg.url(4,24);
	BOOST_TEST(msg.url().path() == "/path/to/resource"sv);
}
BOOST_AUTO_TEST_CASE(method)
{
	std::string data = "GET /path/to/resource?a=b HTTP/1.1\r\n"s;
	req_head_message msg(&data);
	msg.method(0,3);
	BOOST_TEST(msg.method() == "GET"sv);
}
BOOST_AUTO_TEST_SUITE_END() // req_head
BOOST_AUTO_TEST_CASE(resp_head)
{
	std::string data = "200 OK";
	http_parser::resp_head_message msg{200, &data, 4, 2};
	BOOST_TEST(msg.code == 200);
	BOOST_TEST(msg.reason == "OK"sv);

	http_parser::resp_head_message msg_data{&data};
	BOOST_TEST(msg_data.code == 200);
	BOOST_TEST(msg_data.reason == ""sv);
	BOOST_CHECK(msg_data.reason.empty());
}
BOOST_AUTO_TEST_SUITE(headers)
BOOST_AUTO_TEST_CASE(creating)
{
	std::string data = "H: 1\r\nH2: 2";
	auto* mem = std::pmr::get_default_resource();
	header_message<std::pmr::vector, std::string> msg(&data, mem);
	BOOST_CHECK_NO_THROW( msg.add_header_name(0, 1) );
	BOOST_CHECK_NO_THROW( msg.last_header_value(3, 1) );
	BOOST_TEST( msg.find_header("H").value() == "1"sv );
}
BOOST_AUTO_TEST_CASE(empty)
{
	std::string data = "H: 1\r\nH2: 2";
	auto* mem = std::pmr::get_default_resource();
	header_message<std::pmr::vector, std::string> msg(&data, mem);
	BOOST_TEST(msg.empty() == true);
	BOOST_TEST( msg.find_header("H").has_value() == false );
	BOOST_TEST( msg.headers().size() == 0 );
}
BOOST_AUTO_TEST_CASE(container)
{
	std::string str_data = "H: 1\r\nH2: 2";
	std::pmr::vector<std::byte> data;
	for(auto& s:str_data) data.emplace_back(std::byte(s));
	auto* mem = std::pmr::get_default_resource();
	header_message<std::pmr::vector, std::pmr::vector<std::byte>> msg(&data, mem);
	BOOST_TEST( msg.find_header("H"sv).has_value() == false );
}
BOOST_AUTO_TEST_CASE(search)
{
	std::string str_data = "H: 1\r\nH2: 2";
	std::pmr::vector<std::byte> data;
	for(auto& s:str_data) data.emplace_back((std::byte)s);
	auto* mem = std::pmr::get_default_resource();
	header_message<std::pmr::vector, std::pmr::vector<std::byte>> msg(&data, mem);

	BOOST_CHECK_NO_THROW( msg.add_header_name(0, 1) );
	BOOST_TEST(msg.empty() == false);
	BOOST_TEST( msg.headers().size() == 1 );
	BOOST_TEST( msg.find_header("H").value() == ""sv );

	BOOST_CHECK_NO_THROW( msg.last_header_value(3, 1) );
	BOOST_TEST( msg.find_header("H").value() == "1"sv );

	BOOST_TEST( msg.headers().at(0).name == "H"sv );
	BOOST_TEST( msg.headers().at(0).value == "1"sv );

	BOOST_CHECK_NO_THROW( msg.add_header_name(6, 2) );
	BOOST_TEST_REQUIRE( msg.headers().size() == 2 );
	BOOST_TEST( msg.headers().at(1).name == "H2"sv );
}
BOOST_AUTO_TEST_CASE(http_methods)
{
	std::string data = "Transfer-Encoding: chunked\r\nContent-Length: 2809";
	auto* mem = std::pmr::get_default_resource();
	header_message<std::pmr::vector, std::string> msg(&data, mem);

	BOOST_CHECK_NO_THROW( msg.add_header_name(28, 14) );
	BOOST_CHECK_NO_THROW( msg.last_header_value(44, 4) );
	BOOST_TEST( msg.find_header("Content-Length").value() == "2809"sv );
	BOOST_TEST( msg.content_size().value() == 2809 );
	BOOST_TEST( msg.is_chunked() == false );

	BOOST_CHECK_NO_THROW( msg.add_header_name(0, 17) );
	BOOST_CHECK_NO_THROW( msg.last_header_value(19, 7) );
	BOOST_TEST( msg.find_header("Transfer-Encoding").value() == "chunked"sv );
	BOOST_TEST( msg.is_chunked() == true );
}
BOOST_AUTO_TEST_SUITE_END() // headers
BOOST_AUTO_TEST_SUITE(request)
using http1_msg_t = http_parser::http1_message<std::vector, req_head_message, std::string>;
BOOST_AUTO_TEST_CASE(head)
{
	std::string data = "GET /path HTTP/1.1\r\n\r\n"s;
	http1_msg_t msg(&data);

	msg.head().url(4, 5);
	BOOST_TEST(msg.head().url().uri() == "/path"sv);
}
BOOST_AUTO_TEST_CASE(headers)
{
	std::string data = "GET /path HTTP/1.1\r\nTest: test\r\n\r\n"s;
	http1_msg_t msg(&data);
	BOOST_TEST(msg.headers().empty() == true);
}
BOOST_AUTO_TEST_SUITE_END() // request
BOOST_AUTO_TEST_SUITE_END() // messages
BOOST_AUTO_TEST_SUITE_END() // utils
