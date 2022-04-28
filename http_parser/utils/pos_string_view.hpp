#pragma once

/*************************************************************************
 * Copyright © 2022 Hudyaev Alexy <hudyaev.alexy@gmail.com>
 * This file is part of http_parser.
 * Distributed under the MIT License.
 * See accompanying file LICENSE (at the root of this repository)
 *************************************************************************/

#include <span>
#include <string_view>

namespace http_parser {

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

	const Container* underlying_container() const { return src; }

	bool empty() const { return len == 0; }

	std::size_t size() const { return len; }

	void advance_to_end() { assert(src); len = src->size() - pos; }

	basic_position_string_view<Container> substr(std::size_t p) const
	{
		basic_position_string_view<Container> ret(src);
		ret.pos = this->pos + p;
		ret.advance_to_end();
		return ret;
	}
	basic_position_string_view<Container> substr(std::size_t p, std::size_t l) const
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

	void reset()
	{
		pos = 0;
		len = src->size();
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

	auto span() const
	{
		if(!src) return std::span<const value_type>{};
		assert( pos < src->size() && pos + len <= src->size() );
		return std::span( src->data() + pos, len );
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

} // namespace http_parser

