#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE uri

#include <chrono>
#include <boost/regex.hpp>
#include <boost/test/unit_test.hpp>
#include <http_utils/uri_parser.hpp>

namespace utf = boost::unit_test;

constexpr bool enable_speed_tests =
        #ifdef  ENABLE_SPEED_TESTS
        true
        #else
        false
        #endif
        ;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(uri)

struct pmr_boost_traits : http_utils::pmr_narrow_traits {
using regex_type = boost::regex;
using match_type = boost::cmatch;
static inline bool regex_match(const string_type& str, match_type& m, const regex_type& e)
{
	return boost::regex_match(str.c_str(), m, e);
}
static inline auto create_extended(std::pmr::memory_resource*, const char* r)
{
	return boost::regex(r, boost::regex::extended);
}
};

using http_utils::uri_parser;

BOOST_AUTO_TEST_CASE(example)
{
	uri_parser rb("https://user:pa$s@google.com:81/some/path?a=12#b");
	BOOST_TEST(rb.scheme() == "https");
	BOOST_TEST(rb.host() == "google.com");
	BOOST_TEST(rb.port() == 81);
	BOOST_TEST(rb.user() == "user");
	BOOST_TEST(rb.pass() == "pa$s");
	BOOST_TEST(rb.path() == "/some/path");
	BOOST_TEST(rb.request() == "/some/path?a=12");
	BOOST_TEST(rb.anchor() == "b");
	BOOST_TEST(rb.params() == "a=12");
	BOOST_TEST(rb.param("a").value() == "12");
	BOOST_TEST(rb.param("b").has_value() == false);
}
BOOST_AUTO_TEST_CASE(speed, * utf::label("speed") * utf::enable_if<enable_speed_tests>())
{
	using uri_bparser = http_utils::basic_uri_parser<pmr_boost_traits>;
	auto start = std::chrono::high_resolution_clock::now();
	for(std::size_t i=0;i<10'000;++i)
		uri_bparser rb("https://user:pa$s@google.com:81/some/path?a=12#b");
	auto stop = std::chrono::high_resolution_clock::now();
	auto dur = stop - start;
	BOOST_TEST(std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() < 5000);
}
BOOST_AUTO_TEST_CASE(wide)
{
	using http_utils::uri_wparser;
	uri_wparser rb(L"https://user:pa$s@google.com:81/some/path?a=12#b");
	BOOST_CHECK(rb.scheme() == L"https");
	BOOST_CHECK(rb.path() == L"/some/path");
	BOOST_CHECK(rb.param(L"a").value() == L"12");
	BOOST_CHECK(rb.port() == 81);

	BOOST_TEST((uri_wparser(L"http://g.com")).port() == 80);
	BOOST_TEST((uri_wparser(L"https://g.c/s/p?a=1#b")).port() == 443);
	BOOST_TEST((uri_wparser(L"https://u:pa$s@g.c:81/s/p?a=1#b")).port() == 81);
}
BOOST_AUTO_TEST_CASE(request)
{
	BOOST_TEST((uri_parser("http://g.com")).request() == "");
	BOOST_TEST((uri_parser("http://g.com/")).request() == "/");
	BOOST_TEST((uri_parser("http://g.com/a/b")).request() == "/a/b");
	BOOST_TEST((uri_parser("http://g.com/a/b?c")).request() == "/a/b?c");
	BOOST_TEST((uri_parser("http://g.com/a/b?c=21")).request() == "/a/b?c=21");
}
BOOST_AUTO_TEST_CASE(port)
{
	BOOST_TEST((uri_parser("http://google.com")).port() == 80);
	BOOST_TEST((uri_parser("https://google.com/some/path?a=1#b")).port() == 443);
	BOOST_TEST((uri_parser("http://google.com/some/path?a=1#b")).port() == 80);
	BOOST_TEST((uri_parser("http://google.com:81/some/path?a=1#b")).port() == 81);
	BOOST_TEST((uri_parser("http://user:pass@google.com:81/some/path?a=1#b")).port() == 81);
	BOOST_TEST((uri_parser("https://user:pa$s@google.com:81/some/path?a=1#b")).port() == 81);
}
BOOST_AUTO_TEST_CASE(path)
{
	uri_parser rb("https://google.com/some/path?a=1#b");
	BOOST_TEST(rb.path() == "/some/path");
	rb.uri("https://google.com");
	BOOST_TEST(rb.path() == "/");

	BOOST_TEST((uri_parser("http://google.com")).path() == "/");
	BOOST_TEST((uri_parser("http://google.com/a")).path() == "/a");
}
BOOST_AUTO_TEST_CASE(params)
{
	BOOST_TEST((uri_parser("h://g.c")).params() == "");
	BOOST_CHECK((uri_parser("h://g.c")).param("a") == std::nullopt);
	BOOST_TEST((uri_parser("h://g.c/a?a=2")).params() == "a=2");
	BOOST_TEST((uri_parser("h://g.c/?a=2&b=3")).params() == "a=2&b=3");
	BOOST_TEST((uri_parser("h://g.c/?a=2&b=3")).param("a").value() == "2");
	BOOST_TEST((uri_parser("h://g.c/?a=2a&b=3")).param("a").value() == "2a");
	BOOST_TEST((uri_parser("h://g.c/?a=2&b=3")).param("b").value() == "3");
	BOOST_TEST((uri_parser("h://g.c/?a=2&b")).param("b").value() == "");
	BOOST_TEST((uri_parser("h://g.c/?a&b=2")).param("a").value() == "");
	BOOST_CHECK((uri_parser("h://g.c/?a=2&b")).param("c") == std::nullopt);
}
BOOST_AUTO_TEST_CASE(wrong_url)
{
	BOOST_CHECK_THROW(uri_parser("abra"), std::exception);
	BOOST_CHECK_THROW(uri_parser("://g.c"), std::exception);
	BOOST_CHECK_THROW(uri_parser("goo gle.com"), std::exception);
	BOOST_CHECK((uri_parser("h://g.c/?a=2&=")).param("c") == std::nullopt);
	BOOST_TEST((uri_parser("h://g.c/?a=2&=")).param("a").value() == "2");
	BOOST_CHECK((uri_parser("h://g.c/?a&bc")).param("c") == std::nullopt);
}

BOOST_AUTO_TEST_SUITE_END() // uri
BOOST_AUTO_TEST_SUITE_END() // core

