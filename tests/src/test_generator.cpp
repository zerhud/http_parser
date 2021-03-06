#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE generator

#include <boost/test/unit_test.hpp>
#include <http_parser/generator.hpp>
#include <http_parser/utils/factories.hpp>

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
using request_generator = http_parser::basic_generator<http_parser::pmr_string_factory>;
BOOST_AUTO_TEST_SUITE(simple)
BOOST_AUTO_TEST_CASE(example)
{
	using namespace http_parser;
	request_generator gen;
	gen << uri("http://g.c/p/ath?a=1")
	    << header("User-Agent", "Test")
	       ;
	BOOST_TEST(gen.body("content"sv) == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                              "Host: g.c\r\n"
	                              "User-Agent: Test\r\n"
	                              "Content-Length: 7\r\n\r\n"
	                              "content"
	           );

	gen.uri("http://y.d/other/path");
	BOOST_TEST(gen.body("other"sv) == "GET /other/path HTTP/1.1\r\n"
	                              "Host: y.d\r\n"
	                              "User-Agent: Test\r\n"
	                              "Content-Length: 5\r\n\r\n"
	                              "other"
	           );
	BOOST_TEST(gen.body(""sv) == "GET /other/path HTTP/1.1\r\n"
	                           "Host: y.d\r\n"
	                           "User-Agent: Test\r\n\r\n"
	           );
}
BOOST_AUTO_TEST_CASE(example_data_vec)
{
	using request_generator = http_parser::basic_generator<
	    http_parser::pmr_vector_t_factory<std::byte>, std::string_view>;
	using namespace http_parser;
	request_generator gen;
	gen << uri("http://g.c/p/ath?a=1")
	    << header("User-Agent", "Test")
	       ;
	auto result = gen.body("content"sv);
	std::string_view sv_result ( (char*)result.data(), result.size() );
	BOOST_TEST(sv_result == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                        "Host: g.c\r\n"
	                        "User-Agent: Test\r\n"
	                        "Content-Length: 7\r\n\r\n"
	                        "content"sv
	           );
	result = gen.body({(std::byte)0x6f,(std::byte)0x6b});
	sv_result = std::string_view ( (char*)result.data(), result.size() );
	BOOST_TEST(sv_result == "GET /p/ath?a=1 HTTP/1.1\r\n"
	                        "Host: g.c\r\n"
	                        "User-Agent: Test\r\n"
	                        "Content-Length: 2\r\n\r\n"
	                        "ok"sv
	           );
}
BOOST_AUTO_TEST_CASE(memory)
{
	std::pmr::unsynchronized_pool_resource mem;
	request_generator gen(http_parser::pmr_string_factory{&mem});
	gen.uri("http://y.d/other/path");

	std::pmr::string right_body =
	           "GET /other/path HTTP/1.1\r\n"
	           "Host: y.d\r\n"
	           "User-Agent: test\r\n"
	           "Content-Length: 2\r\n\r\n"
	           "ok"
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
BOOST_AUTO_TEST_CASE(just_body_size)
{
	request_generator gen;
	gen.uri("http://g.c");
	BOOST_TEST(gen.body(10) == "GET / HTTP/1.1\r\nHost: g.c\r\nContent-Length: 10\r\n\r\n"sv);
	gen.make_chunked();
	BOOST_TEST(gen.body(10) == "GET / HTTP/1.1\r\nHost: g.c\r\nTransfer-Encoding: chunked\r\n\r\na\r\n"sv);
	BOOST_TEST(gen.body(3) == "3\r\n"sv);
	BOOST_TEST(gen.body(0) == "0\r\n"sv);
}
BOOST_AUTO_TEST_SUITE_END() // simple
BOOST_AUTO_TEST_SUITE(chunked)
BOOST_AUTO_TEST_CASE(headers)
{
	request_generator gen;
	BOOST_TEST(gen.chunked() == false);
	gen.uri("http://g.c/p/a"sv).make_chunked();

	BOOST_TEST(gen.chunked() == true);
	BOOST_TEST(gen.body(""sv) == "GET /p/a HTTP/1.1\r\n"
	           "Host: g.c\r\nTransfer-Encoding: chunked\r\n\r\n"sv);
	BOOST_TEST(gen.body("test"sv) == "4\r\ntest\r\n"sv);
	BOOST_TEST(gen.body("testtesttest"sv) == "c\r\ntesttesttest\r\n"sv);
	BOOST_TEST(gen.body(""sv) == "0\r\n\r\n"sv);
}
BOOST_AUTO_TEST_CASE(with_first_body)
{
	request_generator gen;
	BOOST_TEST(gen.chunked() == false);
	gen.uri("http://g.c/p/a"sv).make_chunked();
	BOOST_TEST(gen.body("ab"sv) == "GET /p/a HTTP/1.1\r\n"
	           "Host: g.c\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nab\r\n"sv);
	BOOST_TEST(gen.body("test"sv) == "4\r\ntest\r\n"sv);
}
BOOST_AUTO_TEST_CASE(body_afetr_and_end)
{
	request_generator gen;
	BOOST_TEST(gen.chunked() == false);
	gen.uri("http://g.c/p/a"sv).make_chunked();
	BOOST_TEST(gen.body(""sv) == "GET /p/a HTTP/1.1\r\n"
	           "Host: g.c\r\nTransfer-Encoding: chunked\r\n\r\n"sv);
	BOOST_TEST(gen.body("testtesttest"sv) == "c\r\ntesttesttest\r\n"sv);
	BOOST_TEST(gen.body(""sv) == "0\r\n\r\n"sv);
}
BOOST_AUTO_TEST_SUITE_END() // chunked
BOOST_AUTO_TEST_CASE(methods)
{
	request_generator gen;
	gen.method(http_parser::methods::post).uri("http://t.d");
	std::pmr::string right_body = "POST / HTTP/1.1\r\nHost: t.d\r\n\r\n";
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
BOOST_AUTO_TEST_CASE(response)
{
	using namespace http_parser;
	request_generator gen;
	gen.response(300, "ok").header("test", "value");
	BOOST_TEST(gen.body("content"sv) == "HTTP/1.1 300 ok\r\n"
	                              "test: value\r\n"
	                              "Content-Length: 7\r\n\r\n"
	                              "content"
	           );
}
BOOST_AUTO_TEST_SUITE_END() // generator
BOOST_AUTO_TEST_SUITE_END() // core
