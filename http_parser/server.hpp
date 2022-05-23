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
class tcp_socket {
public:
	using buffer_t = Buffer;

	virtual ~tcp_socket() noexcept =default ;
//	virtual std::size_t write(buffer_t data) =0 ;
//	virtual bool is_in_progress(std::size_t ind) const =0 ;
};

template<
        template<class> class Container,
        typename DataContainer,
        template<template<class> class,class> class BaseAcceptor,
        std::size_t max_body_size = 4 * 1024,
        std::size_t max_head_size = 1 * 1024
        >
class request_handler {
	using parser_t = http1_parser<Container, DataContainer, BaseAcceptor, max_body_size, max_head_size>;
	parser_t prs;

	struct inner_traits : parser_t::traits_type {
//		std::shared_ptr<user_handler> hndl;
//		Head* head;
//		std::optional<DataContainer> body;

//		DataContainer create_data_container() override ;
//		typename Head::headers_container create_headers_container() override ;

//		void on_head(const Head& head) override {}
//		void on_message(const Head& head, const data_view& body) override {}
//		void on_error(const Head& head, const data_view& body) override {}
	} prs_hndl;

public:
//	request_handler(std::shared_ptr<user_handler> uh)
//	    : prs_hndl{std::move(uh)}
//	    , prs(&prs_hndl)
//	{
//	}

//	const message_t& head() {
//		if(!prs_hndl.head)
//			throw std::logic_error("this mentod of request_handler must be used only inside an callback");
//		return *prs_hndl.head;
//	}

	bool body_ready() const
	{
		return prs_hndl.body.has_value();
	}

	template<typename... Args>
	auto operator()(Args... args)
	{
		return prs(std::forward<Args>(args)...);
	}
};

//TODO: 1. reject by tcp head: ip, port and so on if no handler matched
//         so we need in a method in handler_descriptor_t for check it
template<
        typename IoBuf
      , typename ParserTraitsFactory
      , typename TcpSocket = tcp_socket<IoBuf>
      , typename Hndl = http_request_handler
      , typename HndlDesckBase = handler_descriptor
      >
class server final {
public:
	using buffer_t = IoBuf;
	using traits_factory_t = ParserTraitsFactory;

	using socket_t = TcpSocket;
	using handler_descriptor_t = HndlDesckBase;
	using data_type = typename traits_factory_t::data_type;

	template<typename T>
	using container_t = typename traits_factory_t::container_t<T>;

	using parser_t = typename traits_factory_t::request_parser_t;
	using message_t = parser_t::message_t;

//	using parser_traits_t = std::result_of_t<ptf()>;

	using handler_t = Hndl;
	using handler_ctor_t = std::function<std::shared_ptr<handler_t>(const message_t& req)>;
private:
	struct hndl_t {
		std::unique_ptr<handler_descriptor_t> desck;
	};
	struct active_hndl_t {
		server* self;
		socket_t* sock;
		std::shared_ptr<handler_t> hndl;
		std::optional<parser_t> prs;
		// создать тип - фабрика траитсов, она возращает конкретный тип
		// не нужнно использовать указатель зедсь. этот тип вывести, фабрика
		// указывается в tempalte параетре класса
//		parser_t::traits_type parser_traits;

		void operator()() {}
	};

	std::vector<hndl_t> hndls;
	std::vector<active_hndl_t> actives;
//	ParserTraitsFactory ptf;
public:

//	server(ParserTraitsFactory ptf) : ptf(std::move(ptf)) {}
	~server() noexcept =default ;

	handler_descriptor_t* reg(std::unique_ptr<handler_descriptor_t> hndl)
	{
		return hndls.emplace_back( hndl_t{std::move(hndl)} ).desck.get();
	}

	//TODO: return false if reject socket (see above)
	bool operator()(socket_t* sock)
	{
		for(std::size_t i=0;i<actives.size();++i) {
			if(actives[i].sock == sock) {
				actives[i]();
				return true;
			}
		}
		actives.emplace_back( this, sock )();
		return true;
	}

	std::size_t active_count() const { return actives.size(); }
};

} // namespace http_parser
