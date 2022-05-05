#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE acceptor

#include <chrono>
#include <memory_resource>
#include <boost/test/unit_test.hpp>
#include <http_parser/acceptor.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(acceptor)
BOOST_AUTO_TEST_SUITE(request)
using acceptor_t = http_parser::http1_req_acceptor<std::pmr::vector, std::pmr::string, 100, 100>;
using http1_msg_t = acceptor_t::message_t;
struct test_acceptor : acceptor_t::traits_type {
	std::size_t count = 0;
	std::size_t head_count = 0;
	std::size_t data_created_count = 0;
	std::size_t headers_created_count = 0;
	std::function<void(const http1_msg_t& req_head_message)> head_check;
	std::function<void(const http1_msg_t& req_head_message, const data_view& body)> check;

	std::pmr::string create_data_container() override { ++data_created_count; return std::pmr::string{}; }
	http1_msg_t::headers_container create_headers_container() override {
		++headers_created_count;
		return http1_msg_t::headers_container{};
	}

	void on_head(const http1_msg_t& head) override { ++head_count; if(head_check) head_check(head); }
	void on_request(const http1_msg_t& head, const data_view& body) override {
		++count;
		if(check) check(head, body);
	}
};

BOOST_AUTO_TEST_CASE(head)
{
	test_acceptor traits;
	acceptor_t acceptor( &traits );
	traits.check = [](const http1_msg_t& header, const auto& body) {
		BOOST_TEST(header.head().method() == "DELETE"sv);
		BOOST_TEST(header.head().url().uri() == "/path"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().headers().size() == 1);
		BOOST_TEST(body.size() == 0);
	};
	acceptor("DELETE /path HTTP/1.1\r\nH1:v1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 0);
}
BOOST_AUTO_TEST_CASE(simple_body)
{
	test_acceptor traits;
	acceptor_t acceptor( &traits );
	traits.check = [&traits](const http1_msg_t& header, const auto& body) {
		BOOST_TEST(traits.head_count == 1);
		BOOST_TEST(header.head().method() == "POST"sv);
		BOOST_TEST(header.head().url().uri() == "/pa/th?a=b"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().headers().size() == 2);
		BOOST_TEST(body.size() == 2);
		BOOST_TEST(body == "ok"sv);
	};
	traits.head_check = [&traits](const http1_msg_t& header) {
		BOOST_TEST(traits.count == 0);
		BOOST_TEST(header.head().method() == "POST"sv);
		BOOST_TEST(header.head().url().uri() == "/pa/th?a=b"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().headers().size() == 2);
		BOOST_TEST(header.headers().content_size().value() == 2);
	};
	acceptor("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nContent-Length: 2\r\n\r\nok_extra"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_AUTO_TEST_CASE(chunked_body)
{
	test_acceptor traits;
	acceptor_t acceptor( &traits );
	traits.check = [&traits](const http1_msg_t& header, const auto& body) {
		BOOST_TEST(traits.head_count == 1);
		BOOST_TEST_CONTEXT("current count " << traits.count) {
			if(traits.count == 1) {
				BOOST_TEST(body.size() == 2);
				BOOST_TEST(body == "ok"sv);
			} else if(traits.count == 2) {
				BOOST_TEST(body.size() == 10);
				BOOST_TEST(body == "1234567890"sv);
			} else if(traits.count == 3) {
				BOOST_TEST(body.size() == 0x5a);
				BOOST_TEST(body == "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"sv);
			} else if(traits.count == 4) {
				BOOST_TEST(body.size() == 0);
				BOOST_TEST(body == ""sv);
			} else {
				BOOST_FAIL("the count is "s << traits.count << ' ' << body);
			}
		}
	};
	traits.head_check = [&traits](const http1_msg_t& header) {
		BOOST_TEST(traits.count == 0);
		BOOST_TEST(header.headers().is_chunked() == true);
	};
	acceptor("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nokA\r\n12345678905a"sv);
	BOOST_TEST(traits.count == 2);
	BOOST_TEST(traits.head_count == 1);

	acceptor("\r\n"sv);
	BOOST_TEST(traits.count == 2);
	BOOST_TEST(traits.head_count == 1);

	acceptor("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900\r"sv);
	BOOST_TEST(traits.count == 3);
	BOOST_TEST(traits.head_count == 1);

	acceptor("\n"sv);
	BOOST_TEST(traits.count == 4);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_AUTO_TEST_CASE(head_limit_overflow)
{
	test_acceptor traits;
	acceptor_t acceptor( &traits );
	BOOST_CHECK_THROW( acceptor(
	                       "POST /p HTTP/1.1\r\n"
	                       "H1:1234567890123456789012345678901234567890"
	                       "1234567890123456789012345678901234567890"
	                       "12345678901234567890\r\n"sv), std::out_of_range );
}
BOOST_AUTO_TEST_CASE(body_limit_overflow)
{
	test_acceptor traits;
	acceptor_t acceptor( &traits );
	BOOST_CHECK_NO_THROW( acceptor(
	                       "POST /p HTTP/1.1\r\nContent-Length: 120\r\n\r\n"
	                       "1234567890123456789012345678901234567890"
	                       "1234567890123456789012345678901234567890"
	                       ""sv) );
	BOOST_CHECK_THROW( acceptor("0034567890123456789012345678901234567890"sv), std::out_of_range);
}
BOOST_AUTO_TEST_CASE(head_by_peaces)
{
	test_acceptor traits;
	acceptor_t acceptor( &traits );
	traits.check = [](const http1_msg_t& header, const auto& body) {
		BOOST_TEST(header.head().method() == "DELETE"sv);
		BOOST_TEST(header.head().url().uri() == "/path"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().headers().size() == 1);
		BOOST_TEST(body.size() == 0);
	};
	acceptor("DELETE /path "sv);
	BOOST_TEST(traits.count == 0);
	acceptor("HTTP/1.1\r\nH"sv);
	BOOST_TEST(traits.count == 0);
	acceptor("1:v1\r\n\r"sv);
	BOOST_TEST(traits.count == 0);
	acceptor("\n"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 0);
}
BOOST_AUTO_TEST_SUITE_END() // request

BOOST_AUTO_TEST_SUITE_END() // acceptor
BOOST_AUTO_TEST_SUITE_END() // core
