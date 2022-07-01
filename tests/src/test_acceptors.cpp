#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE utils

#include <chrono>
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <http_parser.hpp>
#include <http_parser/acceptors/ws.hpp>

using namespace std::literals;

BOOST_AUTO_TEST_SUITE(core)
BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_SUITE(acceptors)

using parser_t = http_parser::pmr_str::http1_resp_parser<>;

using qacc_t = parser_t::chain_acceptor_type;
struct test_acc : parser_t::chain_acceptor_type {
	using base_t = parser_t::chain_acceptor_type;

	std::function<bool(const head_t& head)> on_test_;
	std::function<void(const head_t& head)> on_head_;
	std::function<void(const head_t& head, const data_view& body, std::size_t tail)> on_message_;
	std::function<void(const head_t& head, const data_view& body)> on_error_;

	bool can_accept(const head_t& head) { return on_test_ ? on_test_(head) : false;}
	void on_head(const head_t& head) { if(on_head_) on_head_(head); }
	void on_message(const head_t& head, const data_view& body, std::size_t tail) {
		if(on_message_) on_message_(head, body, tail); }
	void on_error(const head_t& head, const data_view& body) {
		if(on_error_) on_error_(head, body);
	}
};

BOOST_AUTO_TEST_SUITE(chain)

struct fixture {
	qacc_t acc;
	std::size_t t1_h_count = 0, t2_h_count = 0;
	std::size_t t1_m_count = 0, t2_m_count = 0;
	std::size_t t1_e_count = 0, t2_e_count = 0;

	test_acc t1, t2;

	std::pmr::string data;
	qacc_t::data_view data_view;
	http_parser::pmr_vector_factory container_factory;
	test_acc::head_t head;

	fixture()
	    : head(&data, container_factory)
	    , data_view(&data)
	{
		t1.on_test_ = [this](const auto& head) { return head.head().code == 10; };
		t2.on_test_ = [this](const auto& head) { return head.head().code == 20; };

		t1.on_head_ = [this](const auto& head){ ++t1_h_count; };
		t2.on_head_ = [this](const auto& head){ ++t2_h_count; };

		t1.on_message_ = [this](const auto& head, auto body, std::size_t tail){ ++t1_m_count; };
		t2.on_message_ = [this](const auto& head, auto body, std::size_t tail){ ++t2_m_count; };

		t1.on_error_ = [this](const auto& head, auto body){ ++t1_e_count; };
		t2.on_error_ = [this](const auto& head, auto body){ ++t2_e_count; };
	}
};

BOOST_FIXTURE_TEST_CASE(on_head, fixture)
{

	acc.add(std::shared_ptr<test_acc>(&t1, [](auto*){}));
	acc.add(std::move(t2));

	BOOST_TEST(acc.chain_size() == 2);


	head.head().code = 10;

	BOOST_TEST(t1_h_count == 0);
	BOOST_TEST(t2_h_count == 0);
	acc.on_head(head);
	BOOST_TEST(t1_h_count == 1);
	BOOST_TEST(t2_h_count == 0);

	head.head().code = 20;
	acc.on_head(head);
	BOOST_TEST(t1_h_count == 1);
	BOOST_TEST(t2_h_count == 1);

	head.head().code = 30;
	acc.on_head(head);
	BOOST_TEST(t1_h_count == 1);
	BOOST_TEST(t2_h_count == 1);
}
BOOST_FIXTURE_TEST_CASE(on_message, fixture)
{

	acc.add(std::shared_ptr<test_acc>(&t1, [](auto*){}));
	acc.add(std::move(t2));

	head.head().code = 10;
	BOOST_TEST(t1_m_count == 0);
	BOOST_TEST(t2_m_count == 0);
	acc.on_message(head, data_view, 0);
	BOOST_TEST(t1_m_count == 1);
	BOOST_TEST(t2_m_count == 0);

	head.head().code = 20;
	acc.on_message(head, data_view, 0);
	BOOST_TEST(t1_m_count == 1);
	BOOST_TEST(t2_m_count == 1);
}
BOOST_FIXTURE_TEST_CASE(on_error, fixture)
{
	acc.add(std::shared_ptr<test_acc>(&t1, [](auto*){}));
	acc.add(std::move(t2));

	head.head().code = 10;
	BOOST_TEST(t1_e_count == 0);
	BOOST_TEST(t2_e_count == 0);
	acc.on_error(head, data_view);
	BOOST_TEST(t1_e_count == 1);
	BOOST_TEST(t2_e_count == 0);

	head.head().code = 20;
	acc.on_error(head, data_view);
	BOOST_TEST(t1_e_count == 1);
	BOOST_TEST(t2_e_count == 1);
}
BOOST_AUTO_TEST_SUITE_END() // chain

BOOST_AUTO_TEST_SUITE(ws)
using parser_t = http_parser::pmr_str::http1_req_parser<>;
template<typename T>
struct handler_factory {
	T operator()(const parser_t::message_t& head)
	{
		return T{};
	}
};
template<typename F>
using ws_tt = http_parser::acceptors::ws<parser_t::message_t, parser_t::data_container_t, F>;
BOOST_AUTO_TEST_CASE(v1)
{
	struct test_ws_acceptor {
		std::size_t msg_count;
		void on_message(parser_t::data_view_t data) {
			BOOST_TEST(data == "abc"sv);
			++msg_count;
		}
	};

	using ftype = handler_factory<test_ws_acceptor>;

	ws_tt<ftype> ws(ftype{});
	parser_t prs(&ws);

	auto data = "GET / HTTP/1.1\r\nUpgrade:ws\r\n\r\n3\r\nabc"s;
	prs(data);

	BOOST_TEST_REQUIRE(ws.handlers.size() == 1);
	BOOST_TEST(ws.handlers.begin()->second.msg_count == 1);
}
BOOST_AUTO_TEST_SUITE_END() // ws

BOOST_AUTO_TEST_SUITE_END() // acceptors
BOOST_AUTO_TEST_SUITE_END() // parser
BOOST_AUTO_TEST_SUITE_END() // core
