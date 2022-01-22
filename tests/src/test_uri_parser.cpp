#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE uri

#include <chrono>
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
	auto start = std::chrono::high_resolution_clock::now();
	for(std::size_t i=0;i<10'000'000;++i)
		uri_parser{"https://user:pa$s@google.com:81/some/path?a=12#b"};
	auto stop = std::chrono::high_resolution_clock::now();
	auto dur = stop - start;
	BOOST_TEST(std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() < 2500);
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
	BOOST_CHECK((uri_wparser(L"http://g.com")).path() == L"/");
	BOOST_TEST((uri_wparser(L"https://g.c/s/p?a=1#b")).port() == 443);
	BOOST_TEST((uri_wparser(L"https://u:pa$s@g.c:81/s/p?a=1#b")).port() == 81);
}
BOOST_AUTO_TEST_CASE(request)
{
	BOOST_TEST((uri_parser("http://g.com")).request() == "");
	BOOST_TEST((uri_parser("http://g.com/")).request() == "/");
	BOOST_TEST((uri_parser("http://g.com/a/b")).request() == "/a/b");
	BOOST_TEST((uri_parser("http://g.com/a/b?c")).params() == "c");
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

	BOOST_TEST((uri_parser("http://g.com/a/b?c")).params() == "c");
	BOOST_TEST((uri_parser("http://g.com/a/b?c")).param("c").value() == "");
}
BOOST_AUTO_TEST_CASE(wrong_url)
{
	BOOST_CHECK((uri_parser("h://g.c/?a=2&=")).param("c") == std::nullopt);
	BOOST_TEST((uri_parser("h://g.c/?a=2&=")).param("a").value() == "2");
	BOOST_CHECK((uri_parser("h://g.c/?a&bc")).param("c") == std::nullopt);
}

BOOST_AUTO_TEST_SUITE(parser)
using uri_parser = http_utils::uri_parser_machine<std::string_view>;
using uri_wparser = http_utils::uri_parser_machine<std::wstring_view>;
BOOST_AUTO_TEST_CASE(full_uri)
{
	uri_parser p, p2;
	p("https://user:pa$s@google.com:81/some/path?a=12#b");
	BOOST_TEST(p.scheme == "https"); BOOST_TEST(p.user == "user");
	BOOST_TEST(p.password == "pa$s");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "81");
	BOOST_TEST(p.path == "/some/path");
	BOOST_TEST(p.path_position == 31);
	BOOST_TEST(p.query == "a=12");
	BOOST_TEST(p.anchor == "b");

	BOOST_CHECK(p == p);
	BOOST_CHECK(p != p2);
	p2("https://user:pa$s@google.com:81/some/path?a=12");
	BOOST_CHECK(p != p2);
	p2("https://user:pa$s@google.com:81/some/path?a=12#b");
	BOOST_CHECK(p == p2);

	uri_wparser wp;
	wp(L"https://user:pa$s@google.com:81/some/path?a=12#b");
	BOOST_CHECK(wp.scheme == L"https");
	BOOST_CHECK(wp.user == L"user");
}
BOOST_AUTO_TEST_CASE(without_scheme)
{
	uri_parser p, p2;
	p("user:pa$s@google.com:81/some/path?a=12#b");
	BOOST_TEST(p.scheme == "");
	BOOST_TEST(p.user == "user");
	BOOST_TEST(p.password == "pa$s");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "81");
	BOOST_TEST(p.path == "/some/path");
	BOOST_TEST(p.query == "a=12");
	BOOST_TEST(p.anchor == "b");
	BOOST_CHECK(p == p2("//user:pa$s@google.com:81/some/path?a=12#b"));
	BOOST_TEST(p2.scheme == "");
	BOOST_TEST(p2.user == "user");
	BOOST_TEST(p2.password == "pa$s");
	BOOST_TEST(p2.domain == "google.com");
}
BOOST_AUTO_TEST_CASE(without_user_pass)
{
	uri_parser p, p2;
	p("google.com:81/some/path?a=12#b");
	BOOST_TEST(p.scheme == "");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "81");
	BOOST_TEST(p.path == "/some/path");
	BOOST_CHECK(p == p2("//google.com:81/some/path?a=12#b"));
}
BOOST_AUTO_TEST_CASE(without_port)
{
	uri_parser p, p2;
	p("google.com/some/path?a=12#b");
	BOOST_TEST(p.scheme == "");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "");
	BOOST_TEST(p.path == "/some/path");
	BOOST_CHECK(p == p2("//google.com/some/path?a=12#b"));
}
BOOST_AUTO_TEST_CASE(without_path)
{
	uri_parser p, p2;
	p("google.com/?a=12#b");
	BOOST_TEST(p.scheme == "");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "");
	BOOST_TEST(p.path == "/");
	BOOST_TEST(p.path_position == 10);
	BOOST_TEST(p.query == "a=12");
	BOOST_TEST(p.anchor == "b");
	BOOST_CHECK(p == p2("//google.com/?a=12#b"));
}
BOOST_AUTO_TEST_CASE(without_query)
{
	uri_parser p, p2;
	p("google.com/#b");
	BOOST_TEST(p.scheme == "");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "");
	BOOST_TEST(p.path == "/");
	BOOST_TEST(p.query == "");
	BOOST_TEST(p.anchor == "b");
	BOOST_CHECK(p == p2("//google.com/#b"));

	p("http://google.com:1/#b");
	BOOST_TEST(p.scheme == "http");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "1");
	BOOST_TEST(p.path == "/");
	BOOST_TEST(p.query == "");
	BOOST_TEST(p.anchor == "b");
}
BOOST_AUTO_TEST_CASE(only_domain)
{
	uri_parser p, p2, p3;
	p("google.com");
	BOOST_TEST(p.scheme == "");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "");
	BOOST_TEST(p.path == "");
	BOOST_TEST(p.path_position == 0);
	BOOST_TEST(p.query == "");
	BOOST_TEST(p.anchor == "");
	BOOST_CHECK(p == p2("//google.com"));

	p("google.com:1");
	BOOST_TEST(p.scheme == "");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "1");
	BOOST_TEST(p.path == "");
	BOOST_TEST(p.query == "");
	BOOST_TEST(p.anchor == "");

	p3("google.com/");
	BOOST_TEST(p3.scheme == "");
	BOOST_TEST(p3.user == "");
	BOOST_TEST(p3.password == "");
	BOOST_TEST(p3.domain == "google.com");
	BOOST_TEST(p3.port == "");
	BOOST_TEST(p3.path == "/");
	BOOST_TEST(p3.query == "");
	BOOST_TEST(p3.anchor == "");
}
BOOST_AUTO_TEST_CASE(query)
{
	uri_parser p;
	p("http://g.com/a/b?c");
	BOOST_TEST(p.scheme == "http");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "g.com");
	BOOST_TEST(p.port == "");
	BOOST_TEST(p.path == "/a/b");
	BOOST_TEST(p.path_position == 12);
	BOOST_TEST(p.query == "c");
	BOOST_TEST(p.anchor == "");
}
BOOST_AUTO_TEST_CASE(path)
{
	uri_parser p;
	p("http://google.com/a");
	BOOST_TEST(p.scheme == "http");
	BOOST_TEST(p.user == "");
	BOOST_TEST(p.password == "");
	BOOST_TEST(p.domain == "google.com");
	BOOST_TEST(p.port == "");
	BOOST_TEST(p.path == "/a");
	BOOST_TEST(p.path_position == 17);
	BOOST_TEST(p.query == "");
	BOOST_TEST(p.anchor == "");
}
BOOST_AUTO_TEST_CASE(wrong)
{
	uri_parser p;
	BOOST_CHECK_THROW(p("http:/"), std::exception);
}
BOOST_AUTO_TEST_SUITE_END() // parser
BOOST_AUTO_TEST_SUITE_END() // uri
BOOST_AUTO_TEST_SUITE_END() // core
