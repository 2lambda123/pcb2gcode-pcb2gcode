#define BOOST_TEST_MODULE units tests
#include <boost/test/included/unit_test.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "units.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(units_tests);

template <typename dimension_t>
dimension_t parse_unit(const std::string& s) {
  po::options_description desc("Allowed options");
  desc.add_options()
      ("test_option", po::value<dimension_t>(), "test option");
  po::variables_map vm;
  po::store(po::command_line_parser(vector<std::string>{"--test_option", s}).options(desc).run(), vm);
  return vm["test_option"].as<dimension_t>();
}

BOOST_AUTO_TEST_CASE(parse_length) {
  BOOST_CHECK_EQUAL(parse_unit<Length>("4").asInch(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Length>("25.4mm").asInch(200), 1);
  BOOST_CHECK_EQUAL(parse_unit<Length>("+50.8mm").asInch(200), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>(" 50.8mm").asInch(200), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>(" 50.8mm    ").asInch(200), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>(" 50.8 mm ").asInch(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>("  \t50.8\tmm\t").asInch(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>("10000thou").asInch(0), 10);
  BOOST_CHECK_EQUAL(parse_unit<Length>("0.254 m").asInch(0), 10);
  Length length;
  std::stringstream ss;
  ss << parse_unit<Length>("4");
  BOOST_CHECK_EQUAL(ss.str(), "4");

  BOOST_CHECK_THROW(parse_unit<Length>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Length>("50.8seconds"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Length>("50.8s"), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_time) {
  BOOST_CHECK_EQUAL(parse_unit<Time>("4").asSecond(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Time>("4s").asSecond(1), 4);
  BOOST_CHECK_EQUAL(parse_unit<Time>("4ms").asSecond(1), 0.004);
  BOOST_CHECK_EQUAL(parse_unit<Time>("-10minutes").asSecond(2), -600);
  BOOST_CHECK_EQUAL(parse_unit<Time>(" 10 min").asSecond(2), 600);
  BOOST_CHECK_EQUAL(parse_unit<Time>(" 10ms").asSecond(2), 0.01);

  BOOST_CHECK_THROW(parse_unit<Time>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>("50.8inches"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>("50.8millimeters"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>("blahblah"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>(""), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_revolution) {
  BOOST_CHECK_EQUAL(parse_unit<Revolution>("4").asRevolution(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Revolution>("4").asRevolution(1), 4);
  BOOST_CHECK_EQUAL(parse_unit<Revolution>("4cycles").asRevolution(100), 4);
  BOOST_CHECK_EQUAL(parse_unit<Revolution>("4 revolutions").asRevolution(100), 4);
  BOOST_CHECK_EQUAL(parse_unit<Revolution>("4 revs").asRevolution(100), 4);

  BOOST_CHECK_THROW(parse_unit<Revolution>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Revolution>("50.8inches"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Revolution>("50.8millimeters"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Revolution>("blahblah"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Revolution>(""), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_rpm) {
  BOOST_CHECK_EQUAL(parse_unit<Rpm>("4").asRpm(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Rpm>("4").asRpm(1), 4);
  BOOST_CHECK_EQUAL(parse_unit<Rpm>("600   cycles\t/\tminute\t\t").asRpm(100), 600);
  BOOST_CHECK_EQUAL(parse_unit<Rpm>("2rotations/s").asRpm(100), 120);
  BOOST_CHECK_EQUAL(parse_unit<Rpm>("4 rpm").asRpm(100), 4);
  BOOST_CHECK_EQUAL(parse_unit<Rpm>("4 RPM").asRpm(100), 4);

  BOOST_CHECK_THROW(parse_unit<Rpm>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Rpm>("50.8inches"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Rpm>("50.8millimeters"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Rpm>("blahblah"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Rpm>(""), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_velocity) {
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("4").asInchPerMinute(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("25.4mm/min").asInchPerMinute(100), 1);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("50.8mm/min").asInchPerMinute(100), 2);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>(" 50.8 mm/min ").asInchPerMinute(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>(" 50.8 mm per min ").asInchPerMinute(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("  \t50.8\tmm\t/minutes").asInchPerMinute(2), 2);

  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8mm"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8 mm "), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8seconds"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8s"), po::validation_error);
}

BOOST_AUTO_TEST_CASE(compare) {
  BOOST_CHECK_LT(parse_unit<Length>("3inch"),  parse_unit<Length>("4inch"));
  BOOST_CHECK_THROW(parse_unit<Length>("3") < parse_unit<Length>("4inch"), comparison_exception);
  BOOST_CHECK_EQUAL(comparison_exception("foo").what(), "foo");
}

BOOST_AUTO_TEST_CASE(parse_MillFeedDirection) {
  BOOST_CHECK_EQUAL(parse_unit<MillFeedDirection::MillFeedDirection>("any"), MillFeedDirection::ANY);
  BOOST_CHECK_EQUAL(parse_unit<MillFeedDirection::MillFeedDirection>("climb"), MillFeedDirection::CLIMB);
  BOOST_CHECK_EQUAL(parse_unit<MillFeedDirection::MillFeedDirection>("clockwise"), MillFeedDirection::CLIMB);
  BOOST_CHECK_EQUAL(parse_unit<MillFeedDirection::MillFeedDirection>("conventional"), MillFeedDirection::CONVENTIONAL);
  BOOST_CHECK_EQUAL(parse_unit<MillFeedDirection::MillFeedDirection>("counterclockwise"), MillFeedDirection::CONVENTIONAL);

  BOOST_CHECK_THROW(parse_unit<MillFeedDirection::MillFeedDirection>("all"), po::validation_error);
}

BOOST_AUTO_TEST_SUITE_END()
