#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE request_generator

#include <boost/test/unit_test.hpp>
#include <http_utils/request_generator.hpp>

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(generator)
using http_utils::request_generator;
BOOST_AUTO_TEST_CASE(example)
{
	request_generator gen;
	gen << uri("http://g.c/p/ath?a=1")
	    << header("User-Agent", "Test")
	    << body("content")
	       ;
	BOOST_TEST(gen.as_string() == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                              "Host:g.c\r\n"
	                              "User-Agent:Test\r\n"
	                              "Content-Length:7\r\n\r\n"
	                              "content\r\n"
	           );

	gen.uri("http://y.d/other/path").body("other");
	BOOST_TEST(gen.as_string() == "GET /other/path HTTP/1.1\r\n"
	                              "Host:y.d\r\n"
	                              "User-Agent:Test\r\n"
	                              "Content-Length:5\r\n\r\n"
	                              "other\r\n"
	           );
}
BOOST_AUTO_TEST_SUITE_END() // generator
BOOST_AUTO_TEST_SUITE_END() // core

