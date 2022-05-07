#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE utils

#include <chrono>
#include <boost/test/unit_test.hpp>
#include <http_parser/utils/cvt.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(to_int_10)
{
	using http_parser::to_int;
	BOOST_TEST(to_int(""sv) == 0);
	BOOST_TEST(to_int("0"sv) == 0);
	BOOST_TEST(to_int("1"sv) == 1);
	BOOST_TEST(to_int("10"sv) == 10);
	BOOST_TEST(to_int("99999999"sv) == 99999999);

	BOOST_TEST(to_int("-1"sv) == -1);
	BOOST_TEST(to_int("-"sv) == 0);
	BOOST_TEST(to_int("-99999999"sv) == -99999999);

	BOOST_TEST(to_int("Af30"sv, 16) == 0xaf30);
	BOOST_TEST(to_int("-a"sv, 16) == -0x0a);
}

BOOST_AUTO_TEST_CASE(to_str_16)
{
	using http_parser::to_str16;
	using http_parser::to_str16c;
	BOOST_TEST(to_str16c<std::string>(0x100A) == "100a"s);
	BOOST_TEST(to_str16c<std::string>(-0xABCD) == "-abcd"s);
	BOOST_TEST(to_str16c<std::string>(0x00) == "0"s);

	BOOST_CHECK(to_str16c<std::wstring>(-0xABCD) == L"-abcd"s);
	BOOST_CHECK(to_str16c<std::wstring>(-0xABCD) == L"-abcd"s);

	std::string to;
	BOOST_TEST(to_str16((std::uint16_t)11, to, true) == "000b");
	to.clear();
	BOOST_TEST(to_str16((std::uint8_t)11, to, true) == "0b");
}

BOOST_AUTO_TEST_CASE(is_hex)
{
	using http_parser::is_hex_digit;
	BOOST_TEST(is_hex_digit('-') == true);

	BOOST_TEST(is_hex_digit('0') == true);
	BOOST_TEST(is_hex_digit('9') == true);
	BOOST_TEST(is_hex_digit('a') == true);
	BOOST_TEST(is_hex_digit('f') == true);
	BOOST_TEST(is_hex_digit('A') == true);
	BOOST_TEST(is_hex_digit('F') == true);

	BOOST_TEST(is_hex_digit('g') == false);
	BOOST_TEST(is_hex_digit('G') == false);
	BOOST_TEST(is_hex_digit(';') == false);
	BOOST_TEST(is_hex_digit('z') == false);
}

BOOST_AUTO_TEST_CASE(to_url_form)
{
	using http_parser::format_to_url;
	std::string to;
	BOOST_TEST(format_to_url(to, "a?\n"sv) == "a%3f%0a"sv);
	to.clear();
	BOOST_TEST(format_to_url(to, "ъ"sv) == "%d1%8a"sv);
}

BOOST_AUTO_TEST_CASE(from_url_form)
{
	using http_parser::format_from_url;
	std::string to;
	BOOST_TEST(format_from_url(to, "a%3f%0a"sv) == "a?\n");
	to.clear();
	BOOST_TEST(format_from_url(to, "%d1%8a"sv) == "ъ");
}

BOOST_AUTO_TEST_SUITE_END() // utils
