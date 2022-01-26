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
#include <charconv>
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
	basic_position_string_view(const Container* s)
	    : basic_position_string_view(s, 0, s->size()) {}
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

	basic_position_string_view& operator = (const basic_position_string_view& other)
	{
		src = other.src;
		pos = other.pos;
		len = other.len;
		return *this;
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

	void advance_to_end() { assert(src); len = src->size() - pos; }

	basic_position_string_view<Container> substr(std::size_t p)
	{
		basic_position_string_view<Container> ret(src);
		ret.pos = this->pos + p;
		ret.advance_to_end();
		return ret;
	}
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
	return out << v.template as<typename OStream::char_type>();
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

template<typename Container>
class basic_response_parser {
public:
	using container_view = basic_position_string_view<Container>;
	using message_type = response_message<Container>;
	using value_type = typename Container::value_type;
	using value_t = typename Container::value_type;
private:
	enum class state {
		begin,
		code, reason,
		header_begin, header_name, header_sep, header_value,
		content_begin, content,
		end
	};

	std::pmr::memory_resource* mem;
	std::function<void(message_type)> callback;
	message_type result;

	state cur_state=state::begin;
	std::size_t cur_pos=0, garbage_pos=0;
	typename Container::value_type cur_symbol;
	container_view parsing;

	template<typename View>
	void advance_parsing(View data)
	{
		auto& con = result.data();
		con.reserve(con.size() + data.size());
		for(std::size_t i=0;i<data.size();++i)
			con.push_back((value_type)data[i]);
		parsing.advance_to_end();
	}

	void parse()
	{
		switch(cur_state) {
		case state::begin: pbegin(); break;
		case state::code: pcode(); break;
		case state::reason: preason(); break;
		case state::header_begin: phbegin(); break;
		case state::header_name: phname(); break;
		case state::header_sep: phsep(); break;
		case state::header_value: phval(); break;
		case state::content_begin: pcontent_begin(); break;
		case state::content: pcontent(); break;
		case state::end: exec_end(); break;
		default: assert(false);
		}
	}
	void to_state(state st)
	{
		parsing = parsing.substr(cur_pos);
		cur_pos = 0;
		cur_state = st;
		assert(parsing.empty() || parsing.back() == result.data().back());
	}
	void pop_back_parsing()
	{
		assert(!parsing.empty());
		assert(cur_pos != 0);
		parsing = parsing.substr(1);
		cur_pos = 0;
		++garbage_pos;
	}
	void pbegin()
	{
		using namespace std::literals;
		if(cur_pos < 9) return;
		if(cur_pos == 9) {
			if(parsing.substr(0,9) == "HTTP/1.1 "sv) to_state(state::code);
			else pop_back_parsing();
		}
		assert( cur_pos == 0 || cur_pos == 9 );
	}
	void pcode()
	{
		if((value_t)0x29 < cur_symbol && cur_symbol < (value_t)0x3A) return;
		auto code = parsing.substr(0, cur_pos).template as<char>();
		auto cr = std::from_chars(code.data(), code.data() + code.size(), result.code);
		if(cr.ec != std::errc()) to_state(state::begin);
		else to_state(state::reason);
	}
	void preason()
	{
		if(cur_symbol == (value_t)0x0D /* \r */ ) {
			result.reason = parsing.substr(1, cur_pos-1);
			to_state(state::header_begin);
		}
	}
	void phbegin()
	{
		using namespace std::literals;
		if(cur_pos == 1) {
			if(parsing.substr(0,2) == "\r\n"sv)
				to_state(state::header_name);
			else to_state(state::begin);
		}
		assert(cur_pos < 2);
	}
	void phname()
	{
		using namespace std::literals;
		if(cur_pos == 1 && parsing.substr(1,2)=="\r\n"sv)
			to_state(state::content_begin);
		else if(cur_symbol == (value_t)0x3A /* : */) {
			result.add_header_name(parsing.substr(1,cur_pos-1).template as<char>());
			to_state(state::header_sep);
		}
	}
	void phsep()
	{
		/* 0x3A - : 0x20 - space */
		assert( cur_pos != 0 || cur_symbol == (value_t)0x3A );
		if(0 < cur_pos && cur_symbol != (value_t)0x20)
			to_state(state::header_value);
	}
	void phval()
	{
		// 0x0D - \r
		if(cur_symbol == (value_t)0x0D) {
			assert(!result.headers_empty());
			result.last_header_value(parsing.substr(0, cur_pos).template as<char>());
			to_state(state::header_begin);
		}
	}
	void pcontent_begin()
	{
		using namespace std::literals;
		if(cur_pos == 1 && parsing.substr(0,2)=="\r\n"sv) {
			auto len_val = result.find_header("Content-Length"sv);
			if(!len_val) {
				to_state(state::end);
				result.content_lenght = 0;
			} else {
				to_state(state::content);
				std::size_t len;
				auto cr = std::from_chars(
				        len_val->data(),
				        len_val->data() + len_val->size(),
				        len);
				if(cr.ec != std::errc()) to_state(state::begin);
				else result.content_lenght = len;
			}
		} else if(1 < cur_pos ) {
			to_state(state::end);
		}
	}
	void pcontent()
	{
		assert(0 < result.content_lenght);
		if(cur_pos < result.content_lenght)
			cur_pos = result.content_lenght-1;
		else if(cur_pos == result.content_lenght) {
			result.content = parsing.substr(1, cur_pos);
			to_state(state::end);
		}
		else assert(false);
	}
	void exec_end()
	{
		assert(callback);
		assert(!parsing.empty());
		assert(!result.data().empty());
		std::size_t tail_size = parsing.size() - 1;
		decltype(result) tmp(std::move(result));
		result = decltype(result)(mem);
		cur_pos = 0;
		garbage_pos = 0;
		assert(tail_size <= tmp.data().size());
		if(tail_size != 0)
			advance_parsing(container_view(&tmp.data()).substr(tmp.data().size() - tail_size));
		parsing = container_view(&result.data());
		to_state(state::begin);
		callback(std::move(tmp));
	}
	void reset_parser()
	{
		parsing = container_view(&result.data());
		cur_pos = 0;
		cur_state = state::begin;
	}
public:
	basic_response_parser(std::function<void(message_type)> cb)
	    : basic_response_parser(std::pmr::get_default_resource(), std::move(cb))
	{
	}
	basic_response_parser(
	        std::pmr::memory_resource* mr,
	        std::function<void(message_type)> cb)
	    : mem(mr)
	    , callback(std::move(cb))
	    , result(mem)
	    , parsing(&result.data())
	{
	}
	template<typename View>
	basic_response_parser& operator()(View data)
	{
		advance_parsing(data);
		for(cur_pos=0;cur_pos<parsing.size();++cur_pos) {
			cur_symbol = parsing[cur_pos];
			parse();
		}
		if(cur_state == state::end) parse();
		return *this;
	}
	void reset()
	{
		result.data().clear();
		garbage_pos = 0;
		reset_parser();
	}
	std::size_t size() const
	{
		return result.data().size();
	}
	std::size_t gabage_index() const
	{
		return garbage_pos;
	}
	// note: all garbage leavs in messasge
	//       and will be delete with it
	void remove_garbage_now()
	{
		Container tmp;
		tmp.swap(result.data());
		for(std::size_t i=garbage_pos;i<tmp.size();++i)
			result.data().push_back(tmp[i]);
		garbage_pos = 0;
		reset_parser();
	}
};

} // namespace http_utils
