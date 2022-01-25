#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <string>
#include <cassert>
#include <optional>
#include <functional>
#include <memory_resource>

namespace http_utils {

template<typename Container>
class basic_position_string_view {
	const Container* src=nullptr;
	std::size_t pos=0;
	std::size_t len=0;
public:
	using value_type = typename Container::value_type;
	using std_string_view = std::basic_string_view<typename Container::value_type>;

	basic_position_string_view() =default ;
	basic_position_string_view(const Container* s) : basic_position_string_view(s, 0, 0) {}
	basic_position_string_view(const Container* s, std::size_t p, std::size_t l)
	    : src(s)
	{
		assign(p, l);
	}

	basic_position_string_view(const Container* s, const basic_position_string_view& other)
	    : src(s)
	    , pos(other.pos)
	    , len(other.len)
	{
	}

	template<typename T>
	basic_position_string_view& operator = (std::basic_string_view<T> sv)
	{
		using vt = typename Container::value_type;
		if(!src) throw std::runtime_error("no strign for assign");
		if((vt*)sv.data() < src->data())
			throw std::runtime_error("no string correlation");
		if(src->size() < (vt*)sv.data() - src->data())
			throw std::runtime_error("no string correlation");
		pos = (vt*)sv.data() - src->data();
		len = sv.size();
		return *this;
	}

	bool empty() const { return len == 0; }

	std::size_t const size() { return len; }

	basic_position_string_view<Container> substr(std::size_t p, std::size_t l)
	{
		basic_position_string_view<Container> ret(src);
		ret.pos = this->pos + p;
		ret.len =  src->size() <= p + l ? src->size() - p : l;
		return ret;
	}

	const value_type& operator[](std::size_t i) const
	{
		return (*src)[pos + i];
	}

	const value_type& back() const
	{
		assert(!empty());
		return *(src->data() + pos + len - 1);
	}

	const value_type& front() const
	{
		assert(!empty());
		return *(src->data() + pos);
	}

	const value_type* data() const
	{
		assert(src != nullptr);
		assert(pos < src->size());
		return src->data() + pos;
	}

	void assign(const Container* s, const basic_position_string_view& other)
	{
		src = s;
		if(src->size() < other.pos + other.len)
			throw std::runtime_error("underline string is too small");
		pos = other.pos;
		len = other.len;
	}

	void assign(const basic_position_string_view& other)
	{
		assign(src, other);
	}

	void assign(Container* s, std::size_t p, std::size_t l)
	{
		src = s;
		if(src) assign(p, l);
	}

	void assign(std::size_t p, std::size_t l)
	{
		if(!src) throw std::runtime_error("cannot assign to positioned string view without source");
		if(src->size() <= p && l != 0)
			throw std::runtime_error("position is too big in positioned string view");
		pos = p;
		std::size_t ctrl = pos + l;
		len = src->size() <= ctrl ? src->size() - pos : l;
	}

	template<typename C>
	std::basic_string_view<C> as() const
	{
		if(!src) return std::basic_string_view<C>{};
		assert( pos < src->size() && pos + len <= src->size() );
		return std::basic_string_view<C>{ (const C*)src->data() + pos, len };
	}

	operator std_string_view() const
	{
		if(!src) return std_string_view{};
		return std_string_view{src->data() + pos, len};
	}
};

template<typename Container, typename Char>
inline bool operator == (basic_position_string_view<Container> l, std::basic_string_view<Char> r)
{
	if(l.size() != r.size()) return false;
	for(std::size_t i=0;i<r.size();++i)
		if(l[i] != (typename Container::value_type)r[i]) return false;
	return true;
}

template<typename OStream, typename Container>
OStream& operator << (OStream& out, const basic_position_string_view<Container>& v)
{
	return out << (typename basic_position_string_view<Container>::std_string_view)v;
}

using pos_string_view = basic_position_string_view<std::pmr::string>;
using pos_data_view = basic_position_string_view<std::pmr::vector<std::byte>>;

template<typename Con>
struct header_view {
	header_view(Con* src) : name(src), value(src) {}

	basic_position_string_view<Con> name;
	basic_position_string_view<Con> value;
};

template<typename Container>
class response_message {
	std::pmr::memory_resource* mem;
	Container data_;

	std::pmr::vector<header_view<Container>> headers_;
	template<typename Source>
	inline void advance_container(Container& to, const Source& from, std::size_t size)
	{
		for(std::size_t i=0;i<size;++i)
			to.push_back((typename Container::value_type)from[i]);
	}
	void move_headers(response_message& other)
	{
		assert(headers_.empty());
		headers_.reserve(other.headers_.size());
		for(auto& h:other.headers_) {
			auto& cur = headers_.emplace_back(&data_);
			cur.name.assign(h.name);
			cur.value.assign(h.value);
		}
	}
public:
	using container_view = basic_position_string_view<Container>;
	response_message(std::pmr::memory_resource* mem)
	    : mem(mem)
	    , data_(mem)
	    , reason(&data_)
	    , content(&data_)
	{}

	response_message(const response_message& other) =delete ;
	response_message(response_message&& other)
	    : mem(other.mem)
	    , data_(std::move(other.data_))
	    , headers_(mem)
	    , code(other.code)
	    , content_lenght(other.content_lenght)
	    , reason(&data_, other.reason)
	    , content(&data_, other.content)
	{
		move_headers(other);
	}

	response_message& operator = (const response_message& other) =delete ;
	response_message& operator = (response_message&& other)
	{
		mem = other.mem;
		data_ = std::move(other.data_);
		headers_ = decltype(headers_)(mem);
		move_headers(other);
		content_lenght = other.content_lenght;
		reason.assign(&data_, other.reason);
		content.assign(&data_, other.content);
		return *this;
	}

	Container& data() { return data_; }
	const Container& data() const { return data_; }
	template<std::size_t N>
	void advance(const char(&chars)[N])
	{
		advance_container(data_, chars, N);
	}

	std::uint16_t code;
	std::size_t content_lenght;
	container_view reason;
	container_view content;

	std::pmr::vector<header_view<Container>> headers() const { return headers_; }
	void add_header_name(std::string_view n)
	{
		headers_.emplace_back(&data_).name = n;
	}
	void last_header_value(std::string_view v)
	{
		headers_.back().value = v;
	}
	bool headers_empty() const { return headers_.empty(); }
	std::optional<std::string_view> find_header(std::string_view v) const
	{
		using hv_type = header_view<Container>;
		auto pos = std::find_if(headers_.begin(), headers_.end(),
		                        [v](const hv_type& hv){ return hv.name == v; });
		if(pos == headers_.end()) return std::nullopt;
		return pos->value.template as<std::string_view::value_type>();
	}
};

class response_parser {
	enum class state {
		begin,
		code, reason,
		header_begin, header_name, header_sep, header_value,
		content_begin, content,
		end
	};

	std::pmr::memory_resource* mem;
	std::function<void(response_message<std::pmr::string>)> callback;
	response_message<std::pmr::string> result;

	state cur_state=state::begin;
	std::size_t cur_pos;
	char cur_symbol;
	std::string_view parsing;

	void advance_parsing(std::string_view data);

	void parse();
	void to_state(state st);
	void pop_back_parsing();
	void pbegin();
	void pcode();
	void preason();
	void phbegin();
	void phname();
	void phsep();
	void phval();
	void pcontent_begin();
	void pcontent();
	void pfinishing();
	void exec_end();
public:
	response_parser(std::function<void(response_message<std::pmr::string>)> cb);
	response_parser(std::pmr::memory_resource* mr, std::function<void(response_message<std::pmr::string>)> cb);
	response_parser& operator()(std::string_view data);
};

} // namespace http_utils
