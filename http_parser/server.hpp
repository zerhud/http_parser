#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <memory>
#include <functional>
#include <type_traits>
#include "parser.hpp"

namespace http_parser {

class http_request_handler {
public:
};

class handler_descriptor {
public:
	virtual ~handler_descriptor () noexcept =default ;
};

template<typename Buffer>
class data_receiver {
public:
	virtual ~data_receiver() noexcept =default ;
	virtual void receive(typename Buffer::value_type* data, std::size_t sz) =0 ;
	virtual void close() =0 ;
	virtual std::size_t max_buffer_size() const =0 ;
};

template<typename Buffer>
class tcp_socket {
public:
	using buffer_t = Buffer;

	virtual ~tcp_socket() noexcept =default ;
	virtual void on_data(data_receiver<Buffer>* dr) =0 ;
//	virtual std::size_t write(buffer_t data) =0 ;
//	virtual bool is_in_progress(std::size_t ind) const =0 ;
};

//TODO: 1. reject by tcp head: ip, port and so on if no handler matched
//         so we need in a method in handler_descriptor_t for check it
template<
        typename IoBuf
      , typename ContainerFactory
      , typename DataContainerFactory
      , std::size_t max_body_size = 4 * 1024
      , std::size_t max_head_size = 1 * 1024
      , typename TcpSocket = tcp_socket<IoBuf>
      , typename Hndl = http_request_handler
      , typename HndlDesckBase = handler_descriptor
      >
class server final {
public:
	using buffer_t = IoBuf;
	using handler_t = Hndl;
	using socket_t = TcpSocket;
	using handler_descriptor_t = HndlDesckBase;
	using data_type = decltype(std::declval<DataContainerFactory>()());
	using parser_t = http1_req_parser<ContainerFactory, DataContainerFactory, max_body_size, max_head_size>;
	using message_t = parser_t::message_t;
	using data_span = std::span<typename buffer_t::value_type>;
private:
	struct hndl_t;
	struct active_hndl_t;
	struct parser_traits : parser_t::traits_type {
		using base_t = parser_t::traits_type;
		using head_t = base_t::head_t;
		using data_view = base_t::data_view;

		server* self;
		active_hndl_t* handler;
		hndl_t* inner_handler;

		void on_head(const head_t& head) override {}
		void on_message(const head_t& head, const data_view& body) override {}
		void on_error(const head_t& head, const data_view& body) override {}
	} ;
	struct inner_parser : data_receiver<buffer_t> {
		parser_traits ptraits;
		parser_t prs;

		inner_parser() : prs(&ptraits) {}

		void receive(data_span data) override { prs(data); }

		void close() override {
			//TODO: finish somehow
		}
	};

	struct hndl_t {
		std::unique_ptr<handler_descriptor_t> desck;
	};
	struct active_hndl_t {
		server* self;
		std::shared_ptr<socket_t> sock;
		std::shared_ptr<handler_t> hndl;
		inner_parser parser;

		active_hndl_t(server* s, std::shared_ptr<socket_t> sock, std::shared_ptr<handler_t> h)
		    : self(s)
		    , sock(sock)
		    , hndl(std::move(h))
		{
			sock->on_data(&parser);
		}
		void operator()(data_span data) {
		}
	};

	std::vector<hndl_t> hndls;
	std::vector<active_hndl_t> actives;
	ContainerFactory cf;
	DataContainerFactory df;
public:

	server() : server(ContainerFactory{}, DataContainerFactory{}) {}
	server(ContainerFactory cf, DataContainerFactory df)
	    : cf(std::move(cf))
	    , df(std::move(df))
	{}

	~server() noexcept =default ;

	handler_descriptor_t* reg(std::unique_ptr<handler_descriptor_t> hndl)
	{
		return hndls.emplace_back( hndl_t{std::move(hndl)} ).desck.get();
	}

	bool operator()(std::shared_ptr<socket_t> sock, data_span data)
	{
		for(std::size_t i=0;i<actives.size();++i) {
			if(actives[i].sock == sock) {
				actives[i](std::move(data));
				return true;
			}
		}
		actives.emplace_back( this, std::move(sock), nullptr )(std::move(data));
		return true;
	}

	//TODO: return false if reject socket (see above)
	bool operator()(std::shared_ptr<socket_t> sock)
	{
		for(std::size_t i=0;i<actives.size();++i) {
			if(actives[i].sock == sock) {
				actives[i]();
				return true;
			}
		}
		actives.emplace_back( this, std::move(sock), nullptr )();
		return true;
	}

	std::size_t active_count() const { return actives.size(); }
};

} // namespace http_parser
