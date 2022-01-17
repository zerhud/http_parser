#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_utils.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <string>
#include <optional>
#include <functional>
#include <memory_resource>

namespace http_utils {

template<typename String>
class basic_position_string_view {
	const String* src=nullptr;
	std::size_t pos=0;
	std::size_t len=0;
public:
	using std_string_view = std::basic_string_view<typename String::value_type>;

	basic_position_string_view() =default ;
	basic_position_string_view(String* s) : basic_position_string_view(s, 0, 0) {}
	basic_position_string_view(String* s, std::size_t p, std::size_t l)
	    : src(s)
	{
		assign(p, l);
	}

	basic_position_string_view(const String* s, const basic_position_string_view& other)
	    : src(s)
	    , pos(other.pos)
	    , len(other.len)
	{
	}

	basic_position_string_view& operator = (std_string_view sv)
	{
		if(!src) throw std::runtime_error("no strign for assign");
		if(sv.data() < src->data())
			throw std::runtime_error("no string correlation");
		if(src->size() < sv.data() - src->data())
			throw std::runtime_error("no string correlation");
		pos = sv.data() - src->data();
		len = sv.size();
		return *this;
	}

	void assign(const String* s, const basic_position_string_view& other)
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

	void assign(String* s, std::size_t p, std::size_t l)
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
		len = src->size() < ctrl ? src->size() - pos : l;
	}

	operator std_string_view() const
	{
		if(!src) return std_string_view{};
		return std_string_view{src->data() + pos, len};
	}
};

template<typename OStream, typename String>
OStream& operator << (OStream& out, const basic_position_string_view<String>& v)
{
	return out << (typename basic_position_string_view<String>::std_string_view)v;
}

using pos_string_view = basic_position_string_view<std::pmr::string>;

struct header_view {
	header_view(std::pmr::string* src) : name(src), value(src) {}

	pos_string_view name;
	pos_string_view value;
};

class response_message {
	std::pmr::memory_resource* mem;
	std::pmr::string data_;

	std::pmr::vector<header_view> headers_;
	void move_headers(response_message& other);
public:
	response_message(std::pmr::memory_resource* mem);

	response_message(response_message&& other);
	response_message(const response_message& other) =delete ;

	response_message& operator = (response_message&& other);
	response_message& operator = (const response_message& other) =delete ;

	std::pmr::string& data() { return data_; }
	const std::pmr::string& data() const { return data_; }

	std::uint16_t code;
	std::size_t content_lenght;
	pos_string_view reason;
	pos_string_view content;

	std::pmr::vector<header_view> headers()const { return headers_; }
	void add_header_name(std::string_view n)
	{
		headers_.emplace_back(&data_).name = n;
	}
	void last_header_value(std::string_view v)
	{
		headers_.back().value = v;
	}
	bool headers_empty() const { return headers_.empty(); }
	std::optional<std::string_view> find_header(std::string_view v)
	{
		auto pos = std::find_if(headers_.begin(), headers_.end(),
		                        [v](auto& hv){ return hv.name == v;});
		if(pos == headers_.end()) return std::nullopt;
		return pos->value;
	}
};

class response_parser {
	enum class state {
		begin,
		code, reason,
		header_begin, header_name, header_sep, header_value,
		content_begin, content,
		finishing, end
	};

	std::pmr::memory_resource* mem;
	std::function<void(response_message)> callback;
	response_message result;

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
	response_parser(std::function<void(response_message)> cb);
	response_parser(std::pmr::memory_resource* mr, std::function<void(response_message)> cb);
	response_parser& operator()(std::string_view data);
};

} // namespace http_utils
