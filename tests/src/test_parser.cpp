#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE parser

#include <chrono>
#include <memory_resource>
#include <boost/test/unit_test.hpp>
#include <http_parser/parser.hpp>
#include <http_parser.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(parser)

BOOST_AUTO_TEST_SUITE(request)

using parser_t = http_parser::pmr_str::http1_req_parser<100>;
using http1_msg_t = parser_t::message_t;
struct test_acceptor : parser_t::acceptor_type {
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();
	std::size_t count = 0;
	std::size_t head_count = 0;
	std::size_t error_count = 0;
	std::function<void(const http1_msg_t& req_head_message)> head_check;
	std::function<void(const http1_msg_t& req_head_message, const data_view& body)> error_check;
	std::function<void(const http1_msg_t& req_head_message, const data_view& body, std::size_t tail)> check;

	bool can_accept(const head_t &head) override { return true; }
	void on_head(const http1_msg_t& head) override { ++head_count; if(head_check) head_check(head); }
	void on_message(const http1_msg_t& head, const data_view& body, std::size_t tail) override {
		++count;
		if(check) check(head, body, tail);
	}
	void on_error(const http1_msg_t& head, const data_view& body) override {
		if(!error_check) BOOST_FAIL("some error detected");
		else {
			++error_count;
			error_check(head, body);
		}
	}
};

struct fixture {
	test_acceptor traits;
	parser_t parser;

	fixture()
	    : parser( &traits )
	{
	}
};

BOOST_FIXTURE_TEST_CASE(head, fixture)
{
	traits.check = [](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(header.head().method() == "DELETE"sv);
		BOOST_TEST(header.head().url().uri() == "/path"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 1);
		BOOST_TEST(body.size() == 0);
	};
	parser("DELETE /path HTTP/1.1\r\nH1:v1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_FIXTURE_TEST_CASE(simple_body, fixture)
{
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(traits.head_count == 1);
		BOOST_TEST(header.head().method() == "POST"sv);
		BOOST_TEST(header.head().url().uri() == "/pa/th?a=b"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 2);
		BOOST_TEST(body.size() == 2);
		BOOST_TEST(body == "ok"sv);
	};
	traits.head_check = [this](const http1_msg_t& header) {
		BOOST_TEST(traits.count == 0);
		BOOST_TEST(header.head().method() == "POST"sv);
		BOOST_TEST(header.head().url().uri() == "/pa/th?a=b"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 2);
		BOOST_TEST(header.headers().content_size().value() == 2);
	};
	parser("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nContent-Length: 2\r\n\r\nok_extra"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_AUTO_TEST_CASE(recreate)
{
	test_acceptor traits;
	parser_t prs(&traits);
	BOOST_TEST(traits.count == 0);
	prs("GET / HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 1);
	parser_t prs2(std::move(prs));
	prs2("GET / HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 2);

	std::optional<parser_t> prs_opt;
	prs_opt.emplace(&traits);
	(*prs_opt)("GET / HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 3);

	prs_opt.reset();
	prs_opt.emplace(&traits);
	(*prs_opt)("GET / HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 4);
}
BOOST_FIXTURE_TEST_CASE(chunked_body, fixture)
{
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
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
	traits.head_check = [this](const http1_msg_t& header) {
		BOOST_TEST(traits.count == 0);
		BOOST_TEST(header.headers().is_chunked() == true);
	};
	parser("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nokA\r\n12345678905a"sv);
	BOOST_TEST(traits.count == 2);
	BOOST_TEST(traits.head_count == 1);

	parser("\r\n"sv);
	BOOST_TEST(traits.count == 2);
	BOOST_TEST(traits.head_count == 1);

	parser("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900\r"sv);
	BOOST_TEST(traits.count == 3);
	BOOST_TEST(traits.head_count == 1);

	parser("\n"sv);
	BOOST_TEST(traits.count == 4);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_FIXTURE_TEST_CASE(chunked_body_trash, fixture)
{
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(traits.head_count == 1);
		BOOST_TEST_CONTEXT("current count " << traits.count) {
			if(traits.count == 1) {
				BOOST_TEST(body.size() == 2);
				BOOST_TEST(body == "ok"sv);
			} else if(traits.count == 2) {
				BOOST_TEST(body.size() == 10);
				BOOST_TEST(body == "1234567890"sv);
			}
		}
	};

	parser("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nTransfer-Encoding: chunked\r\n\r\n2\r\noktrashA\r\n12345678905a"sv);
	BOOST_TEST(traits.count == 2);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_FIXTURE_TEST_CASE(chunked_body_error, fixture)
{
	traits.error_check = [this](const http1_msg_t& header, const auto& body) {
		BOOST_TEST(traits.count == 0);
		BOOST_TEST(traits.head_count == 1);
	};
	parser("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nTransfer-Encoding: chunked\r\n\r\n\r\nok"sv);
	BOOST_TEST(traits.error_count == 1);
}
BOOST_FIXTURE_TEST_CASE(body_limit_overflow, fixture)
{
	BOOST_CHECK_NO_THROW( parser(
	                       "POST /p HTTP/1.1\r\nContent-Length: 120\r\n\r\n"
	                       "1234567890123456789012345678901234567890"
	                       "1234567890123456789012345678901234567890"
	                       ""sv) );
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(traits.count == 1);
		BOOST_TEST( tail == 2 );
		BOOST_TEST( body.size() == 118 );
	};
	parser("12345678901234567890123456789012345678"sv);

	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(traits.count == 2);
		BOOST_TEST(tail == 0);
		BOOST_TEST( body.size() == 2 );
	};
	parser("90"sv);

	BOOST_TEST( traits.count == 2 );

	traits.check = [this](const http1_msg_t& h, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(body.size() == 80);
	};
	parser("POST /p HTTP/1.1\r\nContent-Length: 80\r\n\r\n" // 40 symbols
	         "1234567890123456789012345678901234567890"
	         "1234567890123456789012345678901234567890"
	         ""sv);
	BOOST_TEST( traits.count == 3 );
}
BOOST_FIXTURE_TEST_CASE(head_by_peaces, fixture)
{
	traits.check = [](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(header.head().method() == "DELETE"sv);
		BOOST_TEST(header.head().url().uri() == "/path"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 1);
		BOOST_TEST(body.size() == 0);
	};
	parser("DELETE /path "sv);
	BOOST_TEST(parser.cached_size() == 13);
	BOOST_TEST(traits.count == 0);
	parser("HTTP/1.1\r\nH"sv);
	BOOST_TEST(traits.count == 0);
	parser("1:v1\r\n\r"sv);
	BOOST_TEST(traits.count == 0);
	parser("\n"sv);
	BOOST_TEST(parser.cached_size() == 0);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_FIXTURE_TEST_CASE(memory, fixture)
{
	struct raii {
		raii() {
			std::pmr::set_default_resource(std::pmr::null_memory_resource());
		}
		~raii() {
			std::pmr::set_default_resource(std::pmr::new_delete_resource());
		}
	} mr_setter;
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(traits.head_count == 1);
		BOOST_TEST(header.head().method() == "POST"sv);
		BOOST_TEST(header.head().url().uri() == "/pa/th?a=b"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 2);
		BOOST_TEST(body.size() == 2);
		BOOST_TEST(body == "ok"sv);
	};
	traits.head_check = [this](const http1_msg_t& header) {
		BOOST_TEST(traits.count == 0);
		BOOST_TEST(header.head().method() == "POST"sv);
		BOOST_TEST(header.head().url().uri() == "/pa/th?a=b"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 2);
		BOOST_TEST(header.headers().content_size().value() == 2);
	};
	BOOST_TEST_CHECKPOINT("start parse");
	parser("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nContent-Length: 2\r\n\r\nok_extra"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_FIXTURE_TEST_CASE(few_requests, fixture)
{
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		if(header.head().method() != "DEL"sv) {
			BOOST_TEST(header.headers().size() == 2);
			BOOST_TEST(header.find_header("H1"sv).value() == "v1"sv);
			BOOST_TEST(body.size() == 2);
		}
		if(header.head().method() == "GET"sv) BOOST_TEST(header.head().url() == "/path"sv);
		else if(header.head().method() == "POST"sv) BOOST_TEST(header.head().url() == "/pa/th?a=b"sv);
	};
	BOOST_TEST(parser.cached_size() == 0);
	parser("POST /pa/th?a=b HTTP/1.1\r\nH1:v1\r\nContent-Length: 2\r\n\r\nokGET /path"sv);
	BOOST_TEST(parser.cached_size() == 9);
	BOOST_TEST(traits.count == 1);
	parser(" HTTP/1.1\r\nH1:v1\r\nContent-Length: 2\r\n\r\nokDEL /path HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(parser.cached_size() == 0);
	BOOST_TEST(traits.count == 3);
}
BOOST_FIXTURE_TEST_CASE(just_head, fixture)
{
	traits.head_check = [this](const http1_msg_t& header) {
		BOOST_TEST(header.head().method() == "GET"sv);
		BOOST_TEST(header.head().url() == "/path"sv);
	};
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(header.head().method() == "GET"sv);
		BOOST_TEST(header.head().url() == "/path"sv);
	};
	BOOST_TEST(parser.cached_size() == 0);
	parser("GET /path HTTP/1.1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(parser.cached_size() == 0);
}
BOOST_FIXTURE_TEST_CASE(without_buffer, fixture)
{
	traits.check = [this](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(header.head().method() == "GET"sv);
		BOOST_TEST(header.head().url() == "/path"sv);
	};
	BOOST_TEST(parser.cached_size() == 0);

	auto data = "GET /path HTTP/1.1\r\n\r\n"sv;

	auto buf = parser.create_buf(data.size() + 10);
	BOOST_TEST(parser.cached_size() == data.size() + 10);
	for(std::size_t i=0;i<data.size();++i) buf[i] = data[i];

	parser.trim_buf(data.size());
	BOOST_TEST(parser.cached_size() == data.size());

	parser(0);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(parser.cached_size() == 0);
}
BOOST_AUTO_TEST_SUITE_END() // request

BOOST_AUTO_TEST_SUITE(response)
using parser_t = http_parser::pmr_str::http1_resp_parser<100>;
using http1_msg_t = parser_t::message_t;
struct test_acceptor : parser_t::acceptor_type {
	std::size_t count = 0;
	std::size_t head_count = 0;
	std::function<void(const http1_msg_t& req_head_message)> head_check;
	std::function<void(const http1_msg_t& req_head_message, const data_view& body, std::size_t tail)> check;

	bool can_accept(const head_t &head) override { return true; }
	void on_head(const http1_msg_t& head) override { ++head_count; if(head_check) head_check(head); }
	void on_message(const http1_msg_t& head, const data_view& body, std::size_t tail) override {
		++count;
		if(check) check(head, body, tail);
	}
};
BOOST_AUTO_TEST_CASE(simple_haed)
{
	test_acceptor traits;
	parser_t parser( &traits );
	traits.check = [](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(header.head().code == 300);
		BOOST_TEST(header.head().reason == "TEST"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 1);
		BOOST_TEST(body.size() == 0);
	};
	parser("HTTP/1.1 300 TEST\r\nH1:v1\r\n\r\n"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_AUTO_TEST_CASE(simple_body)
{
	test_acceptor traits;
	parser_t parser( &traits );
	traits.check = [&traits](const http1_msg_t& header, const auto& body, std::size_t tail) {
		BOOST_TEST(tail == 0);
		BOOST_TEST(header.head().code == 250);
		BOOST_TEST(header.head().reason == "OK"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 2);
		BOOST_TEST(body.size() == 2);
		BOOST_TEST(body == "ok"sv);
	};
	traits.head_check = [&traits](const http1_msg_t& header) {
		BOOST_TEST(header.head().code == 250);
		BOOST_TEST(header.head().reason == "OK"sv);
		BOOST_TEST(header.find_header("H1").value() == "v1"sv);
		BOOST_TEST(header.headers().size() == 2);
		BOOST_TEST(header.headers().content_size().value() == 2);
	};
	parser("HTTP/1.1 250 OK\r\nH1:v1\r\nContent-Length: 2\r\n\r\nok_extra"sv);
	BOOST_TEST(traits.count == 1);
	BOOST_TEST(traits.head_count == 1);
}
BOOST_AUTO_TEST_SUITE_END() // response

BOOST_AUTO_TEST_SUITE_END() // parser
BOOST_AUTO_TEST_SUITE_END() // core
