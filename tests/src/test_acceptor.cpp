#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE acceptor

#include <chrono>
#include <memory_resource>
#include <boost/test/unit_test.hpp>
#include <http_parser/acceptor.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(acceptor)

using http_acceptor = http_parser::acceptor<std::pmr::string, std::pmr::vector>;

struct test_acceptor : http_acceptor::traits_type {
	std::size_t http1_resp_cnt = 0;
	std::function<void()> on_resp;

	std::pmr::string create_data_container() override { return std::pmr::string{}; }
	void on_http1_response() override {
		++http1_resp_cnt;
		if(on_resp) on_resp();
		else BOOST_FAIL("on_http_response was called, but no handler was setted");
	}
};

BOOST_AUTO_TEST_CASE(simple)
{
	test_acceptor traits;
	traits.on_resp = [](){};

	http_acceptor acceptor( &traits );

	acceptor("HTTP/1.1 200 OK\r\nH1:v1\r\n\r\n"s);
	BOOST_TEST(traits.http1_resp_cnt == 1);
}

BOOST_AUTO_TEST_SUITE_END() // core
BOOST_AUTO_TEST_SUITE_END() // acceptor
