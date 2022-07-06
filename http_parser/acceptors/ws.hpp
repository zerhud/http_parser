#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <chrono>
#include <unordered_map>

#include "../parser.hpp"


namespace http_parser::acceptors {

template<
        typename Head,
        typename DataContainer,
        typename Factory,
        template<class,class> class Container = std::pmr::unordered_map
        >
struct ws : public http1_parser_chain_acceptor<Head, DataContainer> {
	using base_t = http1_parser_chain_acceptor<Head, DataContainer>;
	using head_t = base_t::head_t;
	using data_view = base_t::data_view;
	using handler_t = decltype(std::declval<Factory>()(std::declval<head_t>()));

	struct handler_info {
		std::optional<handler_t> hndl;
		std::chrono::steady_clock::time_point last_access = std::chrono::steady_clock::now();
	};

	Factory handler_factory;
	Container<const head_t*, handler_info> handlers;

	template<typename... Args>
	ws(Factory&& f, Args... args)
	    : handler_factory(std::forward<Factory>(f))
	    , handlers(std::forward<Args>(args)...)
	{
	}

	bool can_accept(const head_t& head) override {
		using namespace std::literals;
		auto val = head.headers().upgrade_header();
		return val ? val->contains("websocket"sv) : false;
	}
	void on_head(const head_t& head) override {
	}
	void on_message(const head_t& head, const data_view& body, std::size_t tail) override {
		if(!can_accept(head)) return;
		auto& hndl = handlers[&head];
		if(!hndl.hndl) hndl.hndl = create_new(head);
		hndl.last_access = std::chrono::steady_clock::now();
		assert(hndl.hndl.has_value());
		hndl.hndl->on_message(body);
	}
	void on_error(const head_t& head, const data_view& body) override {
	}
private:
	inline auto create_new(const head_t& head) { return handler_factory(head); }
};

} // namespace http_parser::acceptors

