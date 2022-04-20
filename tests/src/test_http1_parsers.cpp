#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE http1_parsers

#include <chrono>
#include <boost/test/unit_test.hpp>
#include <http_parser/utils/headers_parser.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(http1_parsers)
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
BOOST_AUTO_TEST_SUITE_END() // headers
BOOST_AUTO_TEST_SUITE_END() // http1_parsers
BOOST_AUTO_TEST_SUITE_END() // core
