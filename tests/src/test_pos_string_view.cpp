#define BOOST_TEST_DYN_LINK    
#define BOOST_TEST_MODULE pos_string_view

#include <boost/test/unit_test.hpp>
#include <http_parser/utils/pos_string_view.hpp>

using namespace std::literals;
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_SUITE(utils)
BOOST_AUTO_TEST_SUITE(pos_string_view)
using psv_t = http_parser::basic_position_string_view<std::string>;
BOOST_AUTO_TEST_CASE(pos_sv)
{
	std::string src = "hello";
	psv_t t1(&src, 0, 2), t_empty;
	BOOST_TEST((std::string_view)t1 == "he"sv);
	t1.assign(3, 3);
	BOOST_TEST((std::string_view)t1 == "lo"sv);
	BOOST_TEST((std::string_view)t_empty == ""sv);
}
BOOST_AUTO_TEST_CASE(span)
{
	std::string src = "hello";
	psv_t t1(&src, 0, 5), t_empty;
	auto view = t1.span();
	BOOST_TEST(view.size(), 5);
	BOOST_TEST(view.data() == src.data());
	BOOST_TEST(view[1] == 'e');
	BOOST_TEST(view[4] == 'o');
}
BOOST_AUTO_TEST_CASE(assign_to_sv)
{
	std::string src = "test";
	psv_t t1(&src);
	BOOST_CHECK_THROW(t1 = "a"sv, std::exception);
	BOOST_CHECK_NO_THROW((t1 = std::string_view{src.data() + 1, 1}));
	BOOST_TEST(t1 == "e"sv);
}
BOOST_AUTO_TEST_CASE(methods)
{
	std::string src;
	http_parser::basic_position_string_view sv(&src);
	BOOST_TEST( sv.underlying_container() == &src );
	BOOST_TEST(sv.empty() == true);
	BOOST_TEST(((std::string_view)sv).empty() == true);

	src = "test";
	sv.assign(1, 2);
	BOOST_TEST(sv.back() == 's');
	BOOST_TEST(sv.front() == 'e');
	BOOST_TEST(sv.size() == 2);
	BOOST_TEST(sv.data() == &sv.front());
	BOOST_TEST(sv.data()[0] == 'e');
	sv.advance_to_end();
	BOOST_TEST(sv.size() == 3);
	BOOST_TEST(sv == "est"sv);
	sv.assign(0, 4);
	BOOST_TEST(sv.size() == 4);
	BOOST_TEST(sv.substr(1,2) == "es"sv);
	BOOST_TEST(sv.substr(1,3).size() == 3);
	BOOST_TEST(sv.substr(1,3) == "est"sv);
	BOOST_TEST(sv.substr(1,4) == "est"sv);
	BOOST_TEST(sv.substr(1,4).size() == 3);
	BOOST_TEST(sv[1] == 'e');
	BOOST_TEST(sv.substr(2) == "st"sv);
}
BOOST_AUTO_TEST_CASE(creation)
{
	using pos_sv = http_parser::basic_position_string_view<std::string>;
	pos_sv empty;
	BOOST_TEST(empty.empty() == true);
	BOOST_TEST(empty.size() == 0);

	std::string src = "test";
	pos_sv len3(&src, 0, 3);
	BOOST_TEST(len3 == "tes"sv);
}
BOOST_AUTO_TEST_CASE(transformation)
{
	using pos_sv = http_parser::basic_position_string_view<std::vector<std::byte>>;
	std::vector<std::byte> src;
	src.push_back((std::byte)0x74);
	src.push_back((std::byte)0x65);
	src.push_back((std::byte)0x73);
	src.push_back((std::byte)0x74);
	pos_sv sv(&src, 0, 4);
	BOOST_TEST(sv.as<char>() == "test"sv);
}
BOOST_AUTO_TEST_CASE(reset)
{
	auto data = "abc"s;
	psv_t view(&data);
	BOOST_TEST( view == "abc"sv );

	data += "efg"s;
	BOOST_TEST( view == "abc"sv );

	view.reset();
	BOOST_TEST( view == "abcefg"sv );
}
BOOST_AUTO_TEST_CASE(after_finish)
{
	auto data = "abc"s;
	psv_t view1(&data);
	psv_t view2 = view1.following();
	BOOST_CHECK( view2.empty() );
	BOOST_TEST(view2 == ""sv);
	data += "123";
	view2.advance_to_end();
	BOOST_TEST(view2 == "123"sv);
	BOOST_TEST(view1.following() == "123"sv);
}
BOOST_AUTO_TEST_CASE(resize)
{
	auto data = "abc"s;
	psv_t view(&data);
	view.resize(10);
	BOOST_TEST(view == "abc"sv);
	data += "1234567890"s;
	view.resize(10);
	BOOST_TEST(view == "abc1234567"sv);
}
BOOST_AUTO_TEST_CASE(as)
{
	auto data = "abc"s;
	psv_t view(&data);
	auto sv = view.as<char>();
	static_assert(std::is_same_v<decltype(sv), std::string_view>, "as must returns string view");
	BOOST_TEST(sv == "abc"sv);
}
BOOST_AUTO_TEST_CASE(contains)
{
	auto data = "somestrtest"s;
	psv_t view(&data);
	BOOST_TEST(view.contains("s"sv) == true);
	BOOST_TEST(view.contains("q"sv) == false);
	BOOST_TEST(view.contains("str"sv) == true);
	BOOST_TEST(view.contains("strs"sv) == false);
	BOOST_TEST(view.contains("test"sv) == true);
	BOOST_TEST(view.contains("somestrtesta"sv) == false);
}
BOOST_AUTO_TEST_SUITE_END() // pos_string_view
BOOST_AUTO_TEST_SUITE_END() // utils

