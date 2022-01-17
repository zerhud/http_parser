#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE response_parser

#include <chrono>
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <http_utils/response_parser.hpp>

using namespace std::literals;
namespace utf = boost::unit_test;
namespace bdata = boost::unit_test::data;

constexpr bool enable_speed_tests =
        #ifdef  ENABLE_SPEED_TESTS
        true
        #else
        false
        #endif
        ;

BOOST_AUTO_TEST_SUITE(utils)
using psv_t = http_utils::basic_position_string_view<std::string>;
BOOST_AUTO_TEST_CASE(pos_sv)
{
	std::string src = "hello";
	psv_t t1(&src, 0, 2), t_empty;
	BOOST_TEST((std::string_view)t1 == "he"sv);
	t1.assign(3, 3);
	BOOST_TEST((std::string_view)t1 == "lo"sv);
	BOOST_TEST((std::string_view)t_empty == ""sv);
}
BOOST_AUTO_TEST_CASE(assign_to_sv)
{
	std::string src = "test";
	psv_t t1(&src);
	BOOST_CHECK_THROW(t1 = "a"sv, std::exception);
	BOOST_CHECK_NO_THROW((t1 = std::string_view{src.data() + 1, 1}));
	BOOST_TEST(t1 == "e"sv);
}
BOOST_AUTO_TEST_SUITE_END() // utils


BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(responses)

using http_utils::response_parser;
using http_utils::response_message;

BOOST_AUTO_TEST_CASE(move_message)
{
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	response_message from(mem);
	from.data() += "hello";
	from.reason = std::string_view(from.data().data()+1, 2);
	response_message to = std::move(from);

	BOOST_TEST(to.data() == "hello"sv);
	BOOST_TEST(to.reason == "el"sv);

	from = response_message(mem);
	from.data() += "world";
	BOOST_TEST(from.data() == "world"sv);
	from.reason = std::string_view(from.data().data()+1, 2);
	BOOST_TEST(from.reason == "or"sv);
}

BOOST_AUTO_TEST_CASE(example)
{
	std::string_view pack1 = "HTTP/1.1 200 OK\r\nConnection: KeepAlive\r\n"sv;
	std::string_view pack2 = "Content-Length:3\r\n\r\nabc\r\n\r\n"sv;
	std::size_t cnt=0;
	response_parser p([&cnt](response_message msg){
		++cnt;
		BOOST_TEST(msg.code == 200);
		BOOST_TEST(msg.reason == "OK"sv);
		BOOST_TEST(msg.content_lenght == 3);
		BOOST_TEST(msg.content == "abc"sv);
		BOOST_TEST_REQUIRE(msg.headers().size() == 2);
		BOOST_TEST(msg.headers().at(0).name == "Connection"sv);
		BOOST_TEST(msg.headers().at(0).value == "KeepAlive"sv);
		BOOST_TEST(msg.headers().at(1).name == "Content-Length"sv);
		BOOST_TEST(msg.headers().at(1).value == "3"sv);
	});
	p(pack1)(pack2);
	BOOST_TEST(cnt==1);
}
BOOST_DATA_TEST_CASE(two_msgs, bdata::make( {
     "HTTP/1.1 200 OK\r\n\r\n"sv,
     "HTTP/1.1 200 OK\r\n\r\nHTTP/1.1 3"sv,
     "blablablaHTTP/1.1 200 OK\r\n\r\nHTTP/1.1 3"sv
}) ^ bdata::make({
     "HTTP/1.1 300 NY\r\n\r\n"sv,
     "00 NY\r\n\r\n"sv,
     "00 NY\r\n\r\n"sv
}), pack1, pack2)
{
	std::size_t cnt=0;
	response_parser p([&cnt](response_message msg){
		++cnt;
		if(cnt == 1) {
			BOOST_TEST(msg.code == 200);
			BOOST_TEST(msg.reason == "OK"sv);
		} else if(cnt == 2) {
			BOOST_TEST(msg.code == 300);
			BOOST_TEST(msg.reason == "NY"sv);
		}
		BOOST_TEST(msg.content_lenght == 0);
		BOOST_TEST(msg.content == ""sv);
		BOOST_TEST_REQUIRE(msg.headers().size() == 0);
	});
	p(pack1)(pack2);
	BOOST_TEST(cnt==2);
}
BOOST_AUTO_TEST_SUITE_END() // responses
BOOST_AUTO_TEST_SUITE_END() // core
