#include <iostream>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/fiber/all.hpp>
#include <boost/process.hpp>

#include "http_parser.hpp"

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

using namespace std::literals;

namespace integrated_test {

template<std::size_t Size, std::size_t MinTransfer=1>
struct tcp_readwriter {
	    std::array<char, Size> buf;

		template<typename Stream>
		boost::fibers::future<std::string_view> read(Stream& stream)
		{
			    boost::fibers::promise<std::string_view> prom;
				auto ret = prom.get_future();
				net::async_read(
				            stream, net::mutable_buffer(buf.data(), buf.size()),
				            net::transfer_at_least(MinTransfer),
				            [this,prom=std::move(prom)](auto ec,std::size_t tb)mutable{
					    prom.set_value( std::string_view{buf.data(), tb} );
				});
				return ret;
		}

		template<typename Stream>
		boost::fibers::future<void> write(Stream& stream, std::string_view data)
		{
			    boost::fibers::promise<void> prom;
				auto ret = prom.get_future();
				net::async_write(stream, net::buffer(data),
				                 [prom=std::move(prom)](auto ec, std::size_t bt)mutable{
					    prom.set_value();
				});
				return ret;
		}
};

template<typename Interval, typename Fnc, typename Timer = boost::asio::steady_timer>
struct asio_interval_timer {
	    Timer timer;
		Interval interval;
		Fnc fnc;

		asio_interval_timer(boost::asio::io_context& ioc, Interval i, Fnc fnc)
		    : timer(ioc, i)
		    , interval(i)
		    , fnc(std::move(fnc))
		{
			    do_timer();
		}

		void do_timer(boost::system::error_code ec)
		{
			    if(!ec) do_timer();
		}

		void do_timer()
		{
			    fnc();
				timer.expires_after(interval);
				timer.async_wait( [this](auto ec){ do_timer(std::move(ec)); } );
		}
};

template<typename Acceptor, typename Hndl, typename ErrHndl>
struct asio_acceptor {
	     Acceptor acceptor;
		 Hndl hndl;
		 ErrHndl err;

		template<typename Ep>
		asio_acceptor(Acceptor a, Ep ep, Hndl hndl, ErrHndl eh)
		    : acceptor(std::move(a))
		    , hndl(std::move(hndl))
		    , err(std::move(eh))
		{
			    acceptor.open( ep.protocol() );
				acceptor.bind( ep );
				acceptor.listen();

				do_accept();
		}

		void do_accept()
		{
			    acceptor.async_accept( [this](auto ec, auto sock) {
					    if(ec) err(std::move(ec));
						else {
							    hndl(std::move(sock));
								do_accept();
						}
				} );
		}
};

using req_parser = http_parser::http1_req_parser<std::pmr::vector,std::pmr::string>;
struct parser_traits : req_parser::traits_type {

	std::string* gotten_data;
	std::function<void(http_parser::pmr_str::data_type)> writer;

	using message_t = req_parser::message_t;

	std::pmr::string create_data_container() override { return std::pmr::string{}; }
	message_t::headers_container create_headers_container() override { return message_t::headers_container{}; }

	void on_head(const message_t& head) {}
	void on_message(const message_t& head, const data_view& body) {
		*gotten_data = body.as<char>();
		if(writer) {
			http_parser::pmr_str::generator gen;
			writer( gen.response("200"sv, "ok"sv).body("wow, it works"sv) );
		}
	}
	void on_error(const message_t& head, const data_view& body) {}

};

} // namespace integrated_test

int main(int,char**)
{
	using namespace integrated_test;
	boost::asio::io_context ioc;
	asio_interval_timer timer(ioc, 50ms, []{ boost::this_fiber::yield(); } );
	std::thread runner{[&](){ ioc.run(); }};


	std::string gotten_data;

	// listen socket with echo server (http_parser)
	auto echo_fiber = [&ioc,&gotten_data](net::ip::tcp::socket sock) {
		            boost::fibers::fiber([&ioc,sock=std::move(sock),&gotten_data] () mutable {
						    try {
							    tcp_readwriter<512> rw;
								integrated_test::parser_traits traits;
								traits.gotten_data = &gotten_data;
								boost::beast::tcp_stream stream(std::move(sock));
								traits.writer = [&stream,&rw](auto data) mutable {rw.write(stream, data).get();};
								req_parser prs(&traits);
								while(!ioc.stopped()) {
									auto data = rw.read( stream );
									auto data_view = data.get();
									prs(data_view);
								}

						    }
						    catch(const std::exception& ex) { std::cerr << "c++ error: " << ex.what() << std::endl; std::exit(1);}
						    catch(...) { std::cerr << "Unknown error" << std::endl; std::exit(100);}
					}).detach();
	        };

	asio_acceptor accept_1(
	                    net::ip::tcp::acceptor(net::make_strand(ioc)),
	                    net::ip::tcp::endpoint(net::ip::make_address_v4("127.0.0.1"), 8083),
	                    echo_fiber,
	                    [](auto ec){ std::cout << "err: " << ec.message() << std::endl; }
	        );


	std::this_thread::sleep_for(50ms);

	// generate http request to the socket (request_generator)
	// run system curl with same request (with boost.process)
	bool curl_ok = boost::process::system("/usr/bin/env", "curl", "http://localhost:8083", "-d", "hello") == 0;
	if(!curl_ok) std::exit(20);
	if(gotten_data != "hello"sv) std::exit(21);
	// check answer is same

	ioc.stop();
	runner.join();

	return 0;
}
