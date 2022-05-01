#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE acceptor

#include <chrono>
#include <memory_resource>
#include <boost/test/unit_test.hpp>
#include <http_parser/acceptor.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(acceptor)

using http_acceptor = http_parser::acceptor<std::pmr::vector, std::pmr::string>;
using req_msg_t = http_acceptor::request_message_t;

struct test_acceptor : http_acceptor::traits_type {
	std::size_t http1_req_cnt = 0;
	std::size_t http1_resp_cnt = 0;
	std::function<void()> on_resp;
	std::function<void(const req_msg_t& haeder, const std::pmr::string& body)> on_req;

	std::pmr::string create_data_container() override { return std::pmr::string{}; }
	void on_http1_response() override {
		++http1_resp_cnt;
		if(on_resp) on_resp();
		else BOOST_FAIL("on_http_response was called, but no handler was setted");
	}
	void on_http1_request(const req_msg_t& header, const std::pmr::string& body) override
	{
		++http1_req_cnt;
		if(on_req) on_req(header, body);
		else BOOST_FAIL("on_http_request was called, but no handler was setted");
	}
};

BOOST_AUTO_TEST_CASE(simple_req)
{
	test_acceptor traits;
	http_acceptor acceptor( &traits );

	traits.on_req = [](const auto& header, const auto& body){
		BOOST_TEST(body.size() == 0);
	};

	acceptor("GET /path HTTP/1.1\r\nH1:v1\r\n\r\n"s);
	BOOST_TEST(traits.http1_req_cnt == 1);
}

BOOST_AUTO_TEST_CASE(simple_resp)
{
	test_acceptor traits;
	traits.on_resp = [](){};

	http_acceptor acceptor( &traits );

	acceptor("HTTP/1.1 200 OK\r\nH1:v1\r\n\r\n"s);
	BOOST_TEST(traits.http1_resp_cnt == 1);
}

BOOST_AUTO_TEST_SUITE_END() // core
BOOST_AUTO_TEST_SUITE_END() // acceptor
