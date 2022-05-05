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
}

BOOST_AUTO_TEST_SUITE_END() // utils
