#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE uri
      
#include <boost/test/unit_test.hpp>
#include <http_utils/uri_parser.hpp>

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(uri)

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

