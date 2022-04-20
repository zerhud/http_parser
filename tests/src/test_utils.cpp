#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE utils

#include <chrono>
#include <boost/test/unit_test.hpp>
#include <http_parser/utils.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(url_encode)
{
	std::string result;
	http_utils::format_to_url(result, "abc123 ?"sv);
	BOOST_TEST(result == "abc123+%3F");
	result.clear();
}
BOOST_AUTO_TEST_CASE(url_decode)
{
	BOOST_FAIL("no test");
}

BOOST_AUTO_TEST_SUITE_END() // utils
