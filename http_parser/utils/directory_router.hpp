#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <memory_resource>

namespace http_parser {

namespace directory_router_details {

template<typename StringView>
struct caller {

	virtual ~caller(){}
	virtual void call() const =0 ;
	virtual bool match(StringView sv) const =0 ;

	void operator()() { call(); }

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
	  typename ContainerFactory
	, typename MR
	, typename StringView = std::string_view
	, typename Container = decltype(std::declval<ContainerFactory>().template operator()<directory_router_details::caller_ptr<MR, StringView>>())
	>
struct directory_router final {

	directory_router(MR* mr, ContainerFactory cf)
		: mem(mr)
		, factory(cf)
		, routes(cf.template operator()<directory_router_details::caller_ptr<MR, StringView>>())
	{
	}

	template<typename Functor>
	auto& operator()(StringView route, Functor&& fnc)
	{
		struct helper : directory_router_details::caller<StringView> {
			Functor fnc;
			StringView pattern;
			helper(Functor&& fnc, StringView p) : fnc(std::forward<Functor>(fnc)), pattern(p) {}
			void call() const override { fnc(); }
		};
		struct helper_substr : helper {
			helper_substr(Functor&& fnc, StringView p) : helper(std::forward<Functor>(fnc), p) {}
			bool match(StringView sv) const override {
				return directory_router_details::match_substr(helper::pattern, sv);
			}
		};
		struct helper_exactly : helper {
			helper_exactly(Functor&& fnc, StringView p) : helper(std::forward<Functor>(fnc), p) {}
			bool match(StringView sv) const override {
				return helper::pattern == sv;
			}
		};

		directory_router_details::caller_deleter<MR, StringView> deleter(mem, sizeof(helper));
		auto allocated = mem->allocate(sizeof(helper));
		if(route.back() == '/') {
			directory_router_details::caller_ptr ptr(
			        new (allocated) helper_substr(std::forward<Functor>(fnc), route),
			        std::move(deleter)
			        );
			routes.emplace_back(std::move(ptr));
		} else {
			directory_router_details::caller_ptr ptr(
			        new (allocated) helper_exactly(std::forward<Functor>(fnc), route),
			        std::move(deleter)
			        );
			routes.emplace_back(std::move(ptr));
		}
		return *this;
	}

	const auto& operator()(StringView route) const
	{
		for(auto& r:routes) if(r->match(route)) (*r)();
		return *this;
	}
private:
	MR* mem;
	ContainerFactory factory;
	Container routes;
};

} // namespace http_parser
