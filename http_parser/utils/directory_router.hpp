#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <memory_resource>
#include "factories.hpp"

namespace http_parser {

namespace directory_router_details {

template<typename StringView>
struct caller {
	virtual ~caller(){}
	virtual void call(StringView sv) const =0 ;
	virtual bool match(StringView sv) const =0 ;
};

template<typename R, typename S>
struct caller_deleter {
	R* mem;
	std::size_t sz;
	caller_deleter(R* mem, std::size_t sz) : mem(mem), sz(sz) {}
	void operator()(caller<S>* o) const
	{
		o->~caller();
		mem->deallocate(o, sz);
	}
};

template<typename S>
bool match_substr(S pattern, S str)
{
	return pattern.size() <= str.size() && str.substr(0, pattern.size()) == pattern;
}

template<typename R, typename S>
using caller_ptr = std::unique_ptr<caller<S>, caller_deleter<R, S>>;

} // namespace directory_router_details

template<
	  typename ContainerFactory = pmr_vector_factory
	, typename MR = std::pmr::memory_resource
	, typename StringView = std::string_view
	, typename Container = decltype(std::declval<ContainerFactory>().template operator()<directory_router_details::caller_ptr<MR, StringView>>())
	>
struct directory_router final {

	using caller_ptr = directory_router_details::caller_ptr<MR, StringView>;

	directory_router(directory_router&& other)
	    : mem(other.mem)
	    , factory(std::move(other.factory))
	    , routes(factory.template operator()<caller_ptr>())
	{
		routes = std::move(other.routes);
	}

	explicit directory_router(MR* mr = std::pmr::get_default_resource())
	    requires( std::is_default_constructible_v<ContainerFactory> && std::is_same_v<MR, std::pmr::memory_resource> )
	    : directory_router( mr, ContainerFactory{} )
	{}

	directory_router(MR* mr, ContainerFactory cf)
	    : mem(mr)
	    , factory(std::move(cf))
	    , routes(factory.template operator()<caller_ptr>())
	{
	}

	template<typename Functor>
	auto& operator()(StringView route, Functor&& fnc)
	{
		if(route.back() == '/') add_substr(route, std::forward<Functor>(fnc));
		else add_exactly(route, std::forward<Functor>(fnc));
		return *this;
	}

	bool operator()(StringView route) const
	{
		for(auto& r:routes) {
			if(r->match(route))
				return r->call(route), true;
		}
		return false;
	}

	auto size() { return routes.size(); }
private:
	template<typename Functor>
	void add_substr(StringView route, Functor&& fnc)
	{
		struct helper_substr : directory_router_details::caller<StringView> {
			Functor fnc;
			StringView pattern;
			helper_substr(Functor&& fnc, StringView p) : fnc(std::forward<Functor>(fnc)), pattern(p) {}
			bool match(StringView sv) const override {
				return directory_router_details::match_substr(pattern, sv);
			}
			void call(StringView sv) const override {
				if constexpr (requires(StringView s){fnc(s);}) fnc(sv.substr(pattern.size()));
				else fnc();
			}
		};

		add<helper_substr>(route, std::forward<Functor>(fnc));
	}

	template<typename Functor>
	void add_exactly(StringView route, Functor&& fnc)
	{
		struct helper_exactly : directory_router_details::caller<StringView> {
			Functor fnc;
			StringView pattern;
			helper_exactly(Functor&& fnc, StringView p) : fnc(std::forward<Functor>(fnc)), pattern(p) {}
			bool match(StringView sv) const override {
				return pattern == sv;
			}
			void call(StringView sv) const override {
				if constexpr (requires(StringView s){fnc(s);}) fnc(sv);
				else fnc();
			}
		};

		add<helper_exactly>(route, std::forward<Functor>(fnc));
	}

	template<typename T, typename Functor>
	inline void add(StringView route, Functor&& fnc)
	{
		constexpr std::size_t size = sizeof(T);
		auto allocated = mem->allocate(size);
		directory_router_details::caller_deleter<MR, StringView> deleter(mem, size);
		directory_router_details::caller_ptr ptr(
		    new (allocated) T(std::forward<Functor>(fnc), route),
		    std::move(deleter)
		    );
		routes.emplace_back(std::move(ptr));
	}

	MR* mem;
	ContainerFactory factory;
	Container routes;
};

} // namespace http_parser
