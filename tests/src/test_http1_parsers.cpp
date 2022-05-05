#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE http1_parsers

#include <chrono>
#include <boost/test/unit_test.hpp>
#include <http_parser/utils/find.hpp>
#include <http_parser/utils/headers_parser.hpp>
#include <http_parser/utils/http1_head_parsers.hpp>
#include <http_parser/utils/chunked_body_parser.hpp>

using namespace std::literals;
namespace utf = boost::unit_test_framework;

constexpr bool enable_speed_tests =
        #ifdef  ENABLE_SPEED_TESTS
        true
        #else
        false
        #endif
        ;

BOOST_AUTO_TEST_SUITE(utils)
BOOST_AUTO_TEST_SUITE(fast_find)
BOOST_AUTO_TEST_CASE(simple)
{
	std::string data = "abc:abc:";
	auto pos = http_parser::find((std::uint64_t*)data.data(), data.size(), (std::uint8_t)0x3A);
	BOOST_TEST(pos == 3);

	data = "1234567:";
	pos = http_parser::find((std::uint64_t*)data.data(), data.size(), (std::uint8_t)0x3A);
	BOOST_TEST(pos == 7);

	data = "12345678:";
	pos = http_parser::find((std::uint64_t*)data.data(), data.size(), (std::uint8_t)0x3A);
	BOOST_TEST(pos == 8);
}
BOOST_AUTO_TEST_CASE(over)
{
	std::string data = "123456789:";
	auto pos = http_parser::find((std::uint64_t*)data.data(), data.size(), (std::uint8_t)0x3A);
	BOOST_TEST(pos == 9);
}
BOOST_AUTO_TEST_SUITE_END() // fast_find
BOOST_AUTO_TEST_SUITE_END() // utils

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(http1_parsers)
BOOST_AUTO_TEST_SUITE(heads)
BOOST_AUTO_TEST_CASE(sates)
{
	std::string data;
	http_parser::basic_position_string_view view{&data};
	http_parser::http1_request_head_parser prs(view);

	data = "HTTP/1.1 200 OK\r\n"s;
	BOOST_TEST( prs() == http_parser::http1_head_state::http1_resp );
	BOOST_TEST(prs.end_position() == data.size());

	data = "GET /path HTTP/1.1\r"s;
	BOOST_TEST( prs() == http_parser::http1_head_state::wait );
	BOOST_TEST(prs.end_position() == data.size());

	data += "\n";
	BOOST_TEST( prs() == http_parser::http1_head_state::http1_req );
	BOOST_TEST(prs.end_position() == data.size());
}
BOOST_AUTO_TEST_CASE(request_message)
{
	std::string data = "GET /path HTTP/1.1\r\n"s;
	http_parser::basic_position_string_view view{&data};
	http_parser::http1_request_head_parser prs(view);
	prs();
	BOOST_TEST(prs.end_position() == data.size());
	BOOST_TEST(prs.req_msg().method() == "GET"sv);
	BOOST_TEST(prs.req_msg().url().uri() == "/path"sv);
}
BOOST_AUTO_TEST_CASE(response_message)
{
	std::string data = "HTTP/1.1 200 OK OK\r\n"s;
	http_parser::basic_position_string_view view{&data};
	http_parser::http1_request_head_parser prs(view);
	prs();
	BOOST_TEST(prs.end_position() == data.size());
	BOOST_TEST(prs.resp_msg().code == 200);
	BOOST_TEST(prs.resp_msg().reason == "OK OK"sv);
}
BOOST_AUTO_TEST_CASE(max_len)
{
	std::string data = "12345678912345678900"s;
	BOOST_TEST_REQUIRE(data.size() == 20);
	http_parser::basic_position_string_view view{&data};
	http_parser::http1_request_head_parser<std::string, 19> prs(view);
	BOOST_TEST(prs() == http_parser::http1_head_state::garbage);
}
BOOST_AUTO_TEST_CASE(containers)
{
	std::vector<std::byte> data;
	std::string str_data = "HTTP/1.1 200 OK OK\r\nH1:v\r\n"s;
	for(auto& c:str_data) data.emplace_back((std::byte)c);

	http_parser::basic_position_string_view view{&data};
	http_parser::http1_request_head_parser prs(view);
	prs();
	BOOST_TEST(prs.end_position() == data.size()-6);
	BOOST_TEST(prs.resp_msg().code == 200);
	BOOST_TEST(prs.resp_msg().reason == "OK OK"sv);
}
BOOST_AUTO_TEST_SUITE(bugs)
BOOST_AUTO_TEST_CASE(max_size)
{
	std::string data = "GET /path HTTP/1.1\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "Header-With-Long-Value:longvalue long value long value\r\n"
	                   "\r\n"s;
	http_parser::basic_position_string_view view{&data};
	http_parser::http1_request_head_parser prs(view);
	BOOST_TEST(prs() == http_parser::http1_head_state::http1_req);
	BOOST_TEST(prs.end_position() == 20);
	BOOST_TEST(prs.req_msg().method() == "GET"sv);
	BOOST_TEST(prs.req_msg().url().uri() == "/path"sv);
}
BOOST_AUTO_TEST_CASE(correct_http)
{
	std::string data = "GET /path http/1.1\rHTTP/1.1\r\n"s;
	http_parser::basic_position_string_view view{&data};
	http_parser::http1_request_head_parser prs(view);
	BOOST_TEST(prs() == http_parser::http1_head_state::garbage);
}
BOOST_AUTO_TEST_SUITE_END() // bugs
BOOST_AUTO_TEST_SUITE_END() // heads
BOOST_AUTO_TEST_SUITE(headers)
BOOST_AUTO_TEST_CASE(common)
{
	std::string data = "Name: value\r\nother:v\r\n\r\n";
	http_parser::basic_position_string_view view(&data);
	http_parser::headers_parser<std::string, std::vector> prs(view);
	BOOST_TEST( prs() == data.size() );
	BOOST_TEST(prs.is_finished() == true);
	auto res = prs.extract_result();
	BOOST_TEST_REQUIRE(res.headers().size() == 2);
	BOOST_TEST(res.find_header("Name").value() == "value"sv);
	BOOST_TEST(res.find_header("other").value() == "v"sv);
}
BOOST_AUTO_TEST_CASE(by_pieces)
{
	std::string data = "";
	http_parser::basic_position_string_view view(&data);
	http_parser::headers_parser<std::string, std::vector> prs(view);

	BOOST_TEST(prs() == 0);
	BOOST_TEST(prs.is_finished() == false);

	data += "nam";
	BOOST_TEST(prs() == 3);
	BOOST_TEST(prs.is_finished() == false);

	data += "e: v";
	BOOST_TEST(prs() == 7);
	BOOST_TEST(prs.is_finished() == false);

	data += "al\r\n";
	BOOST_TEST(prs() == 12);
	BOOST_TEST(prs.is_finished() == false);

	data += "n2:v\r\n";
	BOOST_TEST(prs() == 18);
	BOOST_TEST(prs.is_finished() == false);

	data += "\r";
	BOOST_TEST(prs() == 18);
	BOOST_TEST(prs.is_finished() == false);

	data += "\n";
	BOOST_TEST(prs() == data.size());
	BOOST_TEST(prs.is_finished() == true);

	auto old = data.size();
	data += "body";
	http_parser::basic_position_string_view data_view(&data, old, 4);
	BOOST_TEST( data_view == "body"sv );

	auto res = prs.extract_result();
	BOOST_TEST_REQUIRE(res.headers().size() == 2);
	BOOST_TEST(res.find_header("name").value() == "val"sv);
	BOOST_TEST(res.find_header("n2").value() == "v"sv);
}
BOOST_AUTO_TEST_CASE(single)
{
	std::string data = "name:value\r\n\r\n";
	http_parser::basic_position_string_view view(&data);
	http_parser::headers_parser<std::string, std::vector> prs(view);
	BOOST_TEST(prs() == data.size());
	BOOST_TEST(prs.is_finished() == true);
	auto res = prs.extract_result();
	BOOST_TEST_REQUIRE(res.headers().size() == 1);
	BOOST_TEST(res.find_header("name").value() == "value"sv);
}
BOOST_AUTO_TEST_CASE(skip_first_bytes)
{
	std::string data = "garbagename:value\r\n\r\n";
	http_parser::basic_position_string_view view(&data);
	http_parser::headers_parser<std::string, std::vector> prs(view);
	prs.skip_first_bytes(7);
	BOOST_TEST(prs() == data.size());
	BOOST_TEST(prs.is_finished() == true);
	auto res = prs.extract_result();
	BOOST_TEST_REQUIRE(res.headers().size() == 1);
	BOOST_TEST(res.headers()[0].name == "name"sv);
}
BOOST_AUTO_TEST_CASE(skip_first_bytes_steps)
{
	std::string data = "garba"s;
	http_parser::basic_position_string_view view(&data);
	http_parser::headers_parser<std::string, std::vector> prs(view);
	prs.skip_first_bytes(7);
	prs();
	BOOST_TEST(prs.is_finished() == false);
	data += "gename:value\r\n\r\n"s;
	BOOST_TEST(prs() == data.size());
	BOOST_TEST(prs.is_finished() == true);
	auto res = prs.extract_result();
	BOOST_TEST_REQUIRE(res.headers().size() == 1);
	BOOST_TEST(res.headers()[0].name == "name"sv);
}
BOOST_AUTO_TEST_CASE(speed, * utf::enable_if<enable_speed_tests>())
{
	std::string data = "name:value\r\nname: value\r\n\r\n";
	http_parser::basic_position_string_view view(&data);
	auto start = std::chrono::high_resolution_clock::now();
	for(std::size_t i=0;i<10'000'000;++i) {
		http_parser::headers_parser<std::string, std::vector> prs(view);
		prs();
	}
	auto end = std::chrono::high_resolution_clock::now();
	BOOST_TEST(std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() < 2'000);
}
BOOST_AUTO_TEST_SUITE_END() // headers
BOOST_AUTO_TEST_SUITE(chunked_body)
BOOST_AUTO_TEST_CASE(simple)
{
	std::string data = "2\r\noka\r\n12345678901\r\na";
	http_parser::basic_position_string_view view(&data);
	http_parser::chunked_body_parser prs(view);
	BOOST_TEST(prs.finish() == false);
	BOOST_TEST(prs.ready() == false);
	for(std::size_t i=1;i<4;++i) {
		BOOST_TEST_CONTEXT("i == " << i) {
			BOOST_TEST(prs() == true);
			BOOST_TEST(prs.finish() == false);
			BOOST_TEST(prs.ready() == true);
			if(i == 1) BOOST_TEST(prs.result() == "ok"sv);
			if(i == 2) BOOST_TEST(prs.result() == "1234567890"sv);
			if(i == 3) BOOST_TEST(prs.result() == "a"sv);
		}
	}

	BOOST_TEST(prs() == false);
	BOOST_TEST(prs.ready() == false);
	BOOST_TEST(prs.finish() == false);

	data += "0\r\n";
	BOOST_TEST(prs() == true);
	BOOST_TEST(prs.result() == ""sv);
	BOOST_TEST(prs.finish() == true);
	BOOST_TEST(prs.ready() == true);

	BOOST_TEST(prs() == false);
	BOOST_TEST(prs.finish() == true);
	BOOST_TEST(prs.ready() == true);
}
BOOST_AUTO_TEST_CASE(by_peaces)
{
	std::string data = "1\r\na2";
	http_parser::basic_position_string_view view(&data);
	http_parser::chunked_body_parser prs(view);
	prs();
	BOOST_TEST(prs.ready() == true);
	BOOST_TEST(prs.finish() == false);
	BOOST_TEST(prs.result() == "a"sv);
	BOOST_TEST(prs.end_pos() == 4);

	prs();
	BOOST_TEST(prs.ready() == false);
	BOOST_TEST(prs.end_pos() == 4);

	data += "\r"sv;
	prs();
	BOOST_TEST(prs.ready() == false);
	BOOST_TEST(prs.end_pos() == 4);

	data += "\n"sv;
	prs();
	BOOST_TEST(prs.ready() == false);
	BOOST_TEST(prs.end_pos() == 4);

	data += "o"sv;
	prs();
	BOOST_TEST(prs.ready() == false);
	BOOST_TEST(prs.end_pos() == 4);

	data += "k"sv;
	prs();
	BOOST_TEST(prs.ready() == true);
	BOOST_TEST(prs.result() == "ok"sv);
	BOOST_TEST(prs.end_pos() == 9);

	data += "0"sv;
	prs();
	BOOST_TEST(prs.ready() == false);
	BOOST_TEST(prs.end_pos() == 9);

	data += "\r"sv;
	prs();
	BOOST_TEST(prs.ready() == false);
	BOOST_TEST(prs.finish() == false);
	BOOST_TEST(prs.end_pos() == 9);

	data += "\n"sv;
	prs();
	BOOST_TEST(prs.ready() == true);
	BOOST_TEST(prs.finish() == true);
	BOOST_TEST(prs.result() == ""sv);
	BOOST_TEST(prs.end_pos() == 12);

}
BOOST_AUTO_TEST_CASE(greater_16_body)
{
	std::string data = "14\r\n12345678901234567890";
	http_parser::basic_position_string_view view(&data);
	http_parser::chunked_body_parser prs(view);
	prs();
	BOOST_TEST(prs.ready() == true);
	BOOST_TEST(prs.finish() == false);
	BOOST_TEST(prs.result() == "12345678901234567890"sv);
	BOOST_TEST(prs.end_pos() == 24);
}
BOOST_AUTO_TEST_CASE(wrong)
{
	std::string data = "\r\na";
	http_parser::basic_position_string_view view(&data);
	http_parser::chunked_body_parser prs(view);
	prs();
	BOOST_TEST(prs.ready() == true);
	BOOST_TEST(prs.finish() == true);
	BOOST_TEST(prs.error() == true);
	BOOST_TEST(prs.result() == ""sv);
}
BOOST_AUTO_TEST_SUITE_END() // chunked_body
BOOST_AUTO_TEST_SUITE_END() // http1_parsers
BOOST_AUTO_TEST_SUITE_END() // core
