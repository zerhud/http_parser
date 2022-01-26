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
BOOST_AUTO_TEST_SUITE(pos_string_view)
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
BOOST_AUTO_TEST_CASE(methods)
{
	std::string src;
	http_utils::basic_position_string_view sv(&src);
	BOOST_TEST(sv.empty() == true);
	BOOST_TEST(((std::string_view)sv).empty() == true);

	src = "test";
	sv.assign(1, 2);
	BOOST_TEST(sv.back() == 's');
	BOOST_TEST(sv.front() == 'e');
	BOOST_TEST(sv.size() == 2);
	BOOST_TEST(sv.data() == &sv.front());
	BOOST_TEST(sv.data()[0] == 'e');
	sv.advance_to_end();
	BOOST_TEST(sv.size() == 3);
	BOOST_TEST(sv == "est"sv);
	sv.assign(0, 4);
	BOOST_TEST(sv.size() == 4);
	BOOST_TEST(sv.substr(1,2) == "es"sv);
	BOOST_TEST(sv.substr(1,3).size() == 3);
	BOOST_TEST(sv.substr(1,3) == "est"sv);
	BOOST_TEST(sv.substr(1,4) == "est"sv);
	BOOST_TEST(sv.substr(1,4).size() == 3);
	BOOST_TEST(sv[1] == 'e');
	BOOST_TEST(sv.substr(2) == "st"sv);
}
BOOST_AUTO_TEST_CASE(creation)
{
	using pos_sv = http_utils::basic_position_string_view<std::string>;
	pos_sv empty;
	BOOST_TEST(empty.empty() == true);
	BOOST_TEST(empty.size() == 0);

	std::string src = "test";
	pos_sv len3(&src, 0, 3);
	BOOST_TEST(len3 == "tes"sv);
}
BOOST_AUTO_TEST_CASE(transformation)
{
	using pos_sv = http_utils::basic_position_string_view<std::vector<std::byte>>;
	std::vector<std::byte> src;
	src.push_back((std::byte)0x74);
	src.push_back((std::byte)0x65);
	src.push_back((std::byte)0x73);
	src.push_back((std::byte)0x74);
	pos_sv sv(&src, 0, 4);
	BOOST_TEST(sv.as<char>() == "test"sv);
}
BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END() // utils


BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(responses)

using response_parser = http_utils::basic_response_parser<std::pmr::string>;
using response_message = http_utils::response_message<std::pmr::string>;

BOOST_AUTO_TEST_SUITE(messages)
BOOST_AUTO_TEST_CASE(headers)
{
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	using resp_msg = http_utils::response_message<std::pmr::vector<std::byte>>;
	resp_msg msg(mem);
	msg.advance("h1:1\r\nh2:2\r\n");
	std::string_view data( (const char*)msg.data().data(), 12 );
	BOOST_TEST(data == "h1:1\r\nh2:2\r\n"sv);
	msg.add_header_name(data.substr(0,2));
	msg.last_header_value(data.substr(3,1));
	BOOST_TEST(msg.find_header("h1").value() == "1"sv);
}
BOOST_AUTO_TEST_CASE(move)
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
BOOST_AUTO_TEST_SUITE_END() // messages

BOOST_AUTO_TEST_CASE(example)
{
	using response_parser = http_utils::basic_response_parser<std::pmr::vector<std::byte>>;
	using response_message = http_utils::response_message<std::pmr::vector<std::byte>>;
	std::string_view pack1 = "HTTP/1.1 200 OK\r\nConnection: KeepAlive\r\n"sv;
	std::string_view pack2 = "Content-Length:3\r\n\r\nabc"sv;
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
BOOST_DATA_TEST_CASE(content, bdata::make( {
     "HTTP/1.1 200 OK\r\nContent-Length:6\r\n\r\ncnt123"sv,
     "HTTP/1.1 200 OK\r\nContent-Length:6\r\n\r\ncnt12"sv,
     "HTTP/1.1 200 OK\r\nContent-Length:6\r\n\r\ncnt"sv,
     "HTTP/1.1 200 OK\r\nContent-Length:6\r\n\r"sv,
     "gHTTP/1.1 200 OK\r\nContent-Length:"sv,
     "HTTP/1.1 200 OK\r\nContent-Length:6\r\n\r\ncn"sv
}) ^ bdata::make({
     "HTTP/1.1 300 NY\r\nContent-Length:6\r\n\r\ncnt123"sv,
     "3HTTP/1.1 300 NY\r\nContent-Length:6\r\n\r\ncnt1"sv,
     "123HTTP/1.1 300 NY\r\nContent-Length:6\r\n\r\ncnt123"sv,
     "\ncnt123HTTP/1.1 300 NY\r\nContent-Length:6\r\n\r\ncnt123"sv,
     "6\r\n\r\ncnt123HTTP/1.1 300 NY\r\nContent-Length:6\r\n\r\ncnt123"sv,
     "t1"sv
}) ^ bdata::make({
     ""sv,
     "23"sv,
     ""sv,
     ""sv,
     ""sv,
     "23HTTP/1.1 300 NY\r\nContent-Length:6\r\n\r\ncnt123"sv
}), pack1, pack2, pack3)
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
		BOOST_TEST(msg.content_lenght == 6);
		BOOST_TEST(msg.content == "cnt123"sv);
		BOOST_TEST_REQUIRE(msg.headers().size() == 1);
	});
	p(pack1)(pack2)(pack3);
	BOOST_TEST(cnt==2);
}
BOOST_AUTO_TEST_CASE(reset)
{
	std::size_t cnt=0;
	auto data = "HTTP/1.1 200 OK\r\n\r\n"sv;
	response_parser p([&cnt,data](response_message msg){
		BOOST_TEST(msg.data() == data);
		BOOST_TEST(msg.data().size() == data.size());
		++cnt;
	});
	p("asdfHTTP/1.1 "sv);
	p.reset();
	p(data);
	BOOST_TEST(cnt==1);
}
BOOST_AUTO_TEST_CASE(garbage)
{
	std::size_t cnt=0;
	auto data = "HTTP/1.1 200 OK\r\n\r\n"sv;
	auto pack1 = data.substr(0, 12);
	auto pack2 = data.substr(12);
	response_parser p([&cnt,data](response_message msg){
		BOOST_TEST(msg.data() == data);
		BOOST_TEST(msg.data().size() == data.size());
		++cnt;
	});
	p("asdf"sv);
	p(pack1);
	BOOST_TEST(cnt==0);
	BOOST_TEST(p.size() == 4 + pack1.size());
	BOOST_TEST(p.gabage_index() == 4);
	p.remove_garbage_now();
	BOOST_TEST(p.gabage_index() == 0);
	p(pack2);
	BOOST_TEST(cnt==1);
}
BOOST_AUTO_TEST_SUITE_END() // responses
BOOST_AUTO_TEST_SUITE_END() // core
