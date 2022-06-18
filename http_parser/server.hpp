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
#include <unordered_map>
#include "http_parser.hpp"

namespace http_parser {

struct tcp_socket {
	virtual ~tcp_socket() noexcept =default ;
	virtual void reject() =0 ;
};

template<
        typename CF = http_parser::pmr_vector_factory
      , typename DF = http_parser::pmr_string_factory
      , typename TcpSocket = tcp_socket
      >
class server {
	struct handler;
public:
	using handler_ptr = std::unique_ptr<handler>;
	using parser_t = http_parser::pmr_str::http1_req_parser<>;
	using message_t = parser_t::message_t;
	using data_view = parser_t::traits_type::data_view;
	using string_view = std::string_view;
	using handler_container = decltype(std::declval<CF>().template operator()<handler_ptr>());
	using tcp_socket_t = TcpSocket;

	using simple_body_handler = std::function<void(const message_t& msg,data_view body)>;
private:
	struct handler {
		server* self;
		std::pmr::string method, path;
		simple_body_handler hndl;

		handler(server* s) : self(s) {assert(self);}
	};

	struct socket_handler : parser_t::traits_type {
		server* self = nullptr;
		handler* hndl = nullptr;
		parser_t prs;
		tcp_socket_t* sock;

		socket_handler(server* s, tcp_socket_t* sock)
		    : self(s)
		    , prs(this, self->data_factory, self->container_factory)
		    , sock(sock)
		{}

		void find_hndl(const head_t& head) {
			assert(self);
			for(auto& hndl:self->handlers) {
				if(hndl->path == head.head().url().path())
					this->hndl = hndl.get();
			}
			//TODO: how to stop prase and remove this socket from this->connections?
			if(!hndl) sock->reject();
		}

		void on_head(const head_t& head) override { find_hndl(head); }
		void on_message(const head_t& head, const data_view& body, std::size_t tail) override
		{
			std::cout << "on_msg" << std::endl;
			if(!hndl) find_hndl(head);
			if(hndl) hndl->hndl(head, body);
		}
		void on_error(const head_t& head, const data_view& body) override
		{
			//TODO: and what we can do?
		}

		template<typename D>
		void parse(D&& data)
		{
			using namespace std::literals;
			std::cout << "here we are " << (hndl ? hndl->path : "[no path]"sv) << std::endl;
			prs(std::forward<D>(data));
		}
	};

	handler_container handlers;
	std::pmr::unordered_map<tcp_socket_t*,socket_handler> connections; //TODO: how to create it?
	CF container_factory;
	DF data_factory;
public:

	server(CF&& cf = CF{}, DF&& df = DF{})
	requires ( std::is_constructible_v<CF> && std::is_constructible_v<DF> )
	    : handlers(cf.template operator()<typename handler_container::value_type>())
	    , container_factory(std::move(cf))
	    , data_factory(std::move(df))
	{
	}

	void reg(string_view method, string_view path, simple_body_handler hndl)
	{
		handlers.emplace_back(std::make_unique<handler>(this));
		handlers.back()->method = method;
		handlers.back()->path = path;
		handlers.back()->hndl = std::move(hndl);
	}

	template<typename D>
	void operator()(tcp_socket_t* sock, D&& data)
	{
		connections.try_emplace(sock, this, sock);
		auto pos = connections.find(sock);
		assert(pos != connections.end());
		if(pos!=connections.end())
			pos->second.parse(std::forward<D>(data));
	}

	[[nodiscrad]] std::size_t open_sockets() const
	{
		return connections.size();
	}

	void close_socket(tcp_socket_t* sock)
	{
		auto pos = connections.find(sock);
		if(pos!=connections.end()) connections.erase(pos);
	}
};


} // namespace http_parser
