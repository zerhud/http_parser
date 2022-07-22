#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE routers

#include <chrono>
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>

#include <http_parser/utils/factories.hpp>
#include <http_parser/utils/directory_router.hpp>

using namespace std::literals;
namespace utf = boost::unit_test;
namespace data = boost::unit_test_framework::data;

BOOST_AUTO_TEST_SUITE(utils)
BOOST_AUTO_TEST_SUITE(router)
BOOST_AUTO_TEST_CASE(directory)
{
	std::pmr::memory_resource* mem = std::pmr::get_default_resource();

	http_parser::directory_router r(mem, http_parser::pmr_vector_factory{});
	std::size_t t1=0, t2=0, t3=0;
	r.add("/test"sv, [&t1]{++t1;})
	 .add("/test/"sv, [&t2]{++t2;})
	 .add("/can_compile"sv, []{})
	 .add("/other/"sv, [&t3](std::string_view tail){ ++t3; BOOST_TEST(tail == "tail"); })
	 .add("/eq"sv, [&t3](std::string_view path){ ++t3; BOOST_TEST(path == "/eq"); })
	 ;

	r("/"sv);
	BOOST_TEST(t1==0);
	BOOST_TEST(t2==0);

	r("/tes"sv);
	BOOST_TEST(t1==0);
	BOOST_TEST(t2==0);

	r("/test"sv);
	BOOST_TEST(t1==1);
	BOOST_TEST(t2==0);

	r("/test/"sv);
	BOOST_TEST(t1==1);
	BOOST_TEST(t2==1);

	r("/test/abc"sv);
	BOOST_TEST(t1==1);
	BOOST_TEST(t2==2);

	r("/eq"sv);
	r("/other/tail"sv);
	BOOST_TEST(t3==2);

	BOOST_TEST(r("/nothing"sv) == false);
	BOOST_TEST(r("/test"sv) == true);
}
BOOST_AUTO_TEST_CASE(creation)
{
	http_parser::directory_router default_construtible;

	std::pmr::unsynchronized_pool_resource mr;
	auto dr = std::pmr::get_default_resource();
	std::pmr::set_default_resource( std::pmr::null_memory_resource() );
	std::shared_ptr<std::pmr::memory_resource> memory_raii(&mr, [dr](auto){
		std::pmr::set_default_resource( dr );
	});

	http_parser::directory_router r((std::pmr::memory_resource*)&mr, http_parser::pmr_vector_factory{&mr});
	BOOST_TEST(r.size() == 0);
	r.add("/test"sv, []{});
	BOOST_TEST(r.size() == 1);

	http_parser::directory_router r2( std::move(r) );
	BOOST_TEST(r2.size() == 1);
	r2.add("/other", []{}).add("/", []{}).add("/ttt", []{});
	BOOST_TEST(r2.size() == 4);
}
BOOST_AUTO_TEST_SUITE_END() // router
BOOST_AUTO_TEST_SUITE_END() // utils
