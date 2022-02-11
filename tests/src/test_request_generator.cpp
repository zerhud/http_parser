#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE request_generator

#include <boost/test/unit_test.hpp>
#include <http_parser/request_generator.hpp>

using namespace std::literals;

void check_string(std::string_view result, std::string_view right)
{
	std::string tresult;
	for(std::size_t i=0;i<result.size();++i) {
		BOOST_TEST_CONTEXT("symbol number " << i << " in " << tresult)
		        BOOST_TEST_REQUIRE(result.at(i) == right.at(i));
		tresult += result[i];
	}
}

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(generator)
using request_generator = http_parser::basic_request_generator<std::pmr::string, std::string_view>;
BOOST_AUTO_TEST_CASE(example)
{
	using namespace http_parser;
	request_generator gen;
	gen << uri("http://g.c/p/ath?a=1")
	    << header("User-Agent", "Test")
	       ;
	BOOST_TEST(gen.body("content"sv) == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                              "Host:g.c\r\n"
	                              "User-Agent:Test\r\n"
	                              "Content-Length:7\r\n\r\n"
	                              "content\r\n"
	           );

	gen.uri("http://y.d/other/path");
	BOOST_TEST(gen.body("other"sv) == "GET /other/path HTTP/1.1\r\n"
	                              "Host:y.d\r\n"
	                              "User-Agent:Test\r\n"
	                              "Content-Length:5\r\n\r\n"
	                              "other\r\n"
	           );
	BOOST_TEST(gen.body(""sv) == "GET /other/path HTTP/1.1\r\n"
	                           "Host:y.d\r\n"
	                           "User-Agent:Test\r\n\r\n"
	           );
}
BOOST_AUTO_TEST_CASE(example_data_vec)
{
	using request_generator = http_parser::basic_request_generator<
	    std::pmr::vector<std::byte>, std::string_view>;
	using namespace http_parser;
	request_generator gen;
	gen << uri("http://g.c/p/ath?a=1")
	    << header("User-Agent", "Test")
	       ;
	auto result = gen.body("content"sv);
	std::string_view sv_result ( (char*)result.data(), result.size() );
	BOOST_TEST(sv_result == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                        "Host:g.c\r\n"
	                        "User-Agent:Test\r\n"
	                        "Content-Length:7\r\n\r\n"
	                        "content\r\n"sv
	           );
	result = gen.body({(std::byte)0x6f,(std::byte)0x6b});
	sv_result = std::string_view ( (char*)result.data(), result.size() );
	BOOST_TEST(sv_result == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                        "Host:g.c\r\n"
	                        "User-Agent:Test\r\n"
	                        "Content-Length:2\r\n\r\n"
	                        "ok\r\n"sv
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

	auto body = gen.header("User-Agent", "test").body("ok"sv);
	BOOST_TEST(body.c_str() == right_body.c_str());
	check_string(body, right_body);
}
BOOST_AUTO_TEST_CASE(methods)
{
	request_generator gen;
	gen.method(http_parser::methods::post).uri("http://t.d");
	std::pmr::string right_body = "POST / HTTP/1.1\r\nHost:t.d\r\n\r\n";
	BOOST_TEST(gen.body(""sv).c_str() == right_body.c_str());
	check_string(gen.body(""sv), right_body);
	BOOST_TEST(to_string_view(http_parser::methods::get) == "GET");
	BOOST_TEST(to_string_view(http_parser::methods::head) == "HEAD");
	BOOST_TEST(to_string_view(http_parser::methods::post) == "POST");
	BOOST_TEST(to_string_view(http_parser::methods::put) == "PUT");
	BOOST_TEST(to_string_view(http_parser::methods::delete_method) == "DELETE");
	BOOST_TEST(to_string_view(http_parser::methods::connect) == "CONNECT");
	BOOST_TEST(to_string_view(http_parser::methods::trace) == "TRACE");
	BOOST_TEST(to_string_view(http_parser::methods::patch) == "PATCH");
}
BOOST_AUTO_TEST_SUITE_END() // generator
BOOST_AUTO_TEST_SUITE_END() // core

