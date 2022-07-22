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
	std::size_t t1=0, t2=0;
	r("/test"sv, [&t1]{++t1;})
	 ("/test/"sv, [&t2]{++t2;})
	 ("/can_compile"sv, []{})
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
}
BOOST_AUTO_TEST_SUITE_END() // router
BOOST_AUTO_TEST_SUITE_END() // utils
