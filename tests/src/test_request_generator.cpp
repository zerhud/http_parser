#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE request_generator

#include <boost/test/unit_test.hpp>
#include <http_utils/request_generator.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(generator)
using http_utils::request_generator;
BOOST_AUTO_TEST_CASE(example)
{
	using namespace http_utils;
	request_generator gen;
	gen << uri("http://g.c/p/ath?a=1")
	    << header("User-Agent", "Test")
	       ;
	BOOST_TEST(gen.body("content") == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                              "Host:g.c\r\n"
	                              "User-Agent:Test\r\n"
	                              "Content-Length:7\r\n\r\n"
	                              "content\r\n"
	           );

	gen.uri("http://y.d/other/path");
	BOOST_TEST(gen.body("other") == "GET /other/path HTTP/1.1\r\n"
	                              "Host:y.d\r\n"
	                              "User-Agent:Test\r\n"
	                              "Content-Length:5\r\n\r\n"
	                              "other\r\n"
	           );
	BOOST_TEST(gen.body("") == "GET /other/path HTTP/1.1\r\n"
	                           "Host:y.d\r\n"
	                           "User-Agent:Test\r\n\r\n"
	           );
}
BOOST_AUTO_TEST_CASE(creation)
{
	BOOST_CHECK_THROW(request_generator{nullptr}, std::exception);
}
BOOST_AUTO_TEST_CASE(memory)
{
	std::pmr::unsynchronized_pool_resource mem;
	request_generator gen(&mem);
	gen.uri("http://y.d/other/path");

	std::pmr::string right_body =
	           "GET /other/path HTTP/1.1\r\n"
	           "Host:y.d\r\n"
	           "User-Agent:test\r\n"
	           "Content-Length:2\r\n\r\n"
	           "ok\r\n"
	            ;

	struct raii {
		raii() {
			std::pmr::set_default_resource(std::pmr::null_memory_resource());
		}
		~raii() {
			std::pmr::set_default_resource(std::pmr::new_delete_resource());
		}
	} mr_setter;

	auto body = gen.header("User-Agent", "test").body("ok");
	BOOST_TEST(body.c_str() == right_body.c_str());
	std::string tresult;
	for(std::size_t i=0;i<body.size();++i) {
		BOOST_TEST_CONTEXT("symbol number " << i << " in " << tresult)
		        BOOST_TEST_REQUIRE(body.at(i) == right_body.at(i));
		tresult += body[i];
	}
}
BOOST_AUTO_TEST_SUITE_END() // generator
BOOST_AUTO_TEST_SUITE_END() // core

