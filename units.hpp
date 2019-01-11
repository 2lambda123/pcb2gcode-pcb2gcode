#ifndef UNITS_HPP
#define UNITS_HPP

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/units/quantity.hpp>
#include <boost/optional.hpp>
#include <boost/units/systems/si.hpp>
#include <boost/units/base_units/metric/minute.hpp>
#include <boost/units/base_units/imperial/inch.hpp>
#include <boost/units/base_units/imperial/thou.hpp>
#include <boost/units/io.hpp>
#include <boost/algorithm/string.hpp>

#include "common.hpp"

struct units_parse_exception : public std::exception {
  units_parse_exception(const std::string& get_what, const std::string& from_what) {
    what_string = "Can't get " + get_what + " from: " + from_what;
  }
  units_parse_exception(const std::string& what) {
    what_string = what;
  }

  virtual const char* what() const throw()
  {
    return what_string.c_str();
  }
 private:
  std::string what_string;
};

struct comparison_exception : public std::exception {
  comparison_exception(const std::string& what) {
    what_string = what;
  }

  virtual const char* what() const throw()
  {
    return what_string.c_str();
  }
 private:
  std::string what_string;
};

// Given a string, provide methods to extract successive numbers,
// words, etc from it.
class Lexer {
 public:
  Lexer(const std::string& s) : pos(0), input(s) {}
  std::string get_whitespace() {
    return get_string<int>(std::isspace);
  }
  std::string get_word() {
    get_whitespace();
    return get_string<int>(std::isalpha);
  }
  double get_double() {
    get_whitespace();
    std::string text = get_string<bool>([](int c) {
        return std::isdigit(c) || c == '-' || c == '.' || c == '+';
      });
    try {
      return boost::lexical_cast<double>(text);
    } catch (boost::bad_lexical_cast e) {
      throw units_parse_exception("double", text);
    }
  }
  void get_division() {
    get_whitespace();
    if (!get_exact("/") && !get_exact("per")) {
      throw units_parse_exception("double", input.substr(pos));
    }
  }
  bool at_end() {
    return pos == input.size();
  }
  size_t pos;
 private:
  // Gets all characters from current position until the first that
  // doesn't pass test_fn or end of input.
  template <typename test_return_type>
  std::string get_string(test_return_type (*test_fn)(int)) {
    size_t start = pos;
    while (pos < input.size() && test_fn(input[pos])) {
      pos++;
    }
    return input.substr(start, pos-start);
  }
  // Returns number of characters advanced if string is found at
  // current position.  If not found, returns 0.
  int get_exact(const std::string& s) {
    if (input.compare(pos, s.size(), s) == 0) {
      pos += s.size();
      return s.size();
    }
    return 0;
  }
  std::string input;
};

template <typename dimension_t>
class UnitBase {
 public:
  typedef boost::units::quantity<dimension_t> quantity;
  typedef dimension_t dimension;
  UnitBase(double value = 0, boost::optional<quantity> one = boost::none) : value(value), one(one) {}

  double asDouble() const {
    return value;
  }
  friend std::ostream& operator<<(std::ostream& s, const UnitBase<dimension_t>& length) {
    if (length.one) {
      s << length.value * *length.one;
    } else {
      s << length.value;
    }
    return s;
  }
  bool operator<(const UnitBase<dimension_t>& other) const {
    if (std::isinf(this->value) || this->value == 0 ||
        std::isinf(other.value) || other.value == 0) {
      // inf, -inf, and zero times anything is unchanged so the units don't matter
      return this->value < other.value;
    } else if (!this->one && !other.one) {
      return this->value < other.value;
    } else if (this->one && other.one) {
      return this->value * *this->one < other.value * *other.one;
    } else {
      throw comparison_exception("Can't compare with units and without.");
    }
  }
  bool operator>=(const UnitBase<dimension_t>& other) const {
    return !(*this < other);
  }
  bool operator==(const UnitBase<dimension_t>& other) const {
    return (*this >= other && other >= *this);
  }

 protected:
  double as(double factor, quantity wanted_unit) const {
    if (!one) {
      // We don't know the units so just use whatever factor was supplied.
      return value*factor;
    }
    return value*(*one)/wanted_unit;
  }
  double value;
  boost::optional<quantity> one;
};

// dimension_t is "length" or "velocity", for example.
template<typename dimension_t>
class Unit : public UnitBase<dimension_t> {};

// Any non-SI base units that you want to use go here.
const boost::units::quantity<boost::units::si::length> inch(1*boost::units::imperial::inch_base_unit::unit_type());
const boost::units::quantity<boost::units::si::length> thou(1*boost::units::imperial::thou_base_unit::unit_type());
const boost::units::quantity<boost::units::si::time> minute(1*boost::units::metric::minute_base_unit::unit_type());

struct revolution_base_dimension :
    boost::units::base_dimension<revolution_base_dimension, 1> {};
typedef revolution_base_dimension::dimension_type revolution_type;

struct revolution_base_unit :
    boost::units::base_unit<revolution_base_unit, revolution_type, 1> {
  static std::string name() {return "revolution";}
  static std::string symbol() {return "rev";}
};
typedef revolution_base_unit::unit_type revolution_unit;
const boost::units::quantity<revolution_unit> revolution(1*revolution_base_unit::unit_type());

struct rpm_base_dimension :
    boost::units::base_dimension<rpm_base_dimension, 2> {};
typedef rpm_base_dimension::dimension_type rpm_type;

struct rpm_base_unit :
    boost::units::base_unit<rpm_base_unit, rpm_type, 2> {
  static std::string name() {return "rpm";}
  static std::string symbol() {return "rpm";}
};
typedef rpm_base_unit::unit_type rpm_unit;
const boost::units::quantity<rpm_unit> rpm(1*rpm_base_unit::unit_type());

// shortcuts for Units defined below.
typedef Unit<boost::units::si::length> Length;
typedef Unit<boost::units::si::time> Time;
typedef Unit<revolution_unit> Revolution;
typedef Unit<boost::units::si::velocity> Velocity;
typedef Unit<rpm_unit> Rpm;

template<>
class Unit<boost::units::si::length> : public UnitBase<boost::units::si::length> {
 public:
  Unit(double value = 0, boost::optional<quantity> one = boost::none) : UnitBase(value, one) {}
  double asInch(double factor) const {
    return as(factor, inch);
  }
  static quantity get_unit(Lexer& lex) {
    std::string unit = lex.get_word();
    if (unit == "mm" ||
        unit == "millimeter" ||
        unit == "millimeters") {
      return boost::units::si::meter/1000.0;
    }
    if (unit == "m" ||
        unit == "meter" ||
        unit == "meters`") {
      return 1 * boost::units::si::meter;
    }
    if (unit == "in" ||
        unit == "inch" ||
        unit == "inches") {
      return inch;
    }
    if (unit == "thou" ||
        unit == "thous" ||
        unit == "mil" ||
        unit == "mils") {
      return thou;
    }
    throw units_parse_exception("length units", unit);
  }
  Length operator-() const {
    return Length(-value, one);
  }
};

template<>
class Unit<boost::units::si::time> : public UnitBase<boost::units::si::time> {
 public:
  Unit(double value = 0, boost::optional<quantity> one = boost::none) : UnitBase(value, one) {}
  double asSecond(double factor) const {
    return as(factor, 1.0*boost::units::si::second);
  }
  double asMillisecond(double factor) const {
    return as(factor, boost::units::si::second/1000.0);
  }
  static quantity get_unit(Lexer& lex) {
    std::string unit = lex.get_word();
    if (unit == "s" ||
        unit == "second" ||
        unit == "seconds") {
      return 1.0*boost::units::si::second;
    }
    if (unit == "ms" ||
        unit == "millisecond" ||
        unit == "milliseconds" ||
        unit == "millis") {
      return boost::units::si::second/1000.0;
    }
    if (unit == "min" ||
        unit == "mins" ||
        unit == "minute" ||
        unit == "minutes") {
      return minute;
    }
    throw units_parse_exception("time units", unit);
  }
};

template<>
class Unit<revolution_unit> : public UnitBase<revolution_unit> {
 public:
  Unit(double value = 0, boost::optional<quantity> one = boost::none) : UnitBase(value, one) {}
  double asRevolution(double factor) const {
    return as(factor, 1.0*revolution);
  }
  static quantity get_unit(Lexer& lex) {
    std::string unit = lex.get_word();
    if (unit == "rotation" ||
        unit == "rotations" ||
        unit == "revolutions" ||
        unit == "revolution" ||
        unit == "rev" ||
        unit == "revs" ||
        unit == "cycle" ||
        unit == "cycles") {
      return 1.0*revolution;
    }
    throw units_parse_exception("revolution units", unit);
  }
};

template<>
class Unit<boost::units::si::velocity> : public UnitBase<boost::units::si::velocity> {
 public:
  Unit(double value = 0, boost::optional<quantity> one = boost::none) : UnitBase(value, one) {}
  double asInchPerMinute(double factor) const {
    return as(factor, inch/minute);
  }
  static quantity get_unit(Lexer& lex) {
    // It's either "length/time" or "length per time".
    Length::quantity numerator;
    Time::quantity denominator;
    numerator = Length::get_unit(lex);
    lex.get_division();
    denominator = Time::get_unit(lex);
    return numerator/denominator;
  }
};


static inline boost::units::quantity<rpm_unit>
operator/(const boost::units::quantity<revolution_unit>& lhs,
          const boost::units::quantity<boost::units::si::time>& rhs) {
  return (lhs/revolution)/(rhs/minute) * rpm;
}

template<>
class Unit<rpm_unit> : public UnitBase<rpm_unit> {
 public:
  Unit(double value = 0, boost::optional<quantity> one = boost::none) : UnitBase(value, one) {}
  using UnitBase::as;
  double asRpm(double factor) const {
    return as(factor, 1.0 * rpm);
  }
  static quantity get_unit(Lexer& lex) {
    // It's either "revolution/time" or "revolution per time" or "rpm".
    auto old_pos = lex.pos;
    std::string unit = lex.get_word();
    printf("unit name is %s\n", unit.c_str());
    if (unit == "rpm" ||
        unit == "RPM") {
      return 1.0*rpm;
    }
    lex.pos = old_pos;

    Revolution::quantity numerator;
    Time::quantity denominator;
    numerator = Revolution::get_unit(lex);
    lex.get_division();
    denominator = Time::get_unit(lex);
    return numerator/denominator;
  }
};

template <typename unit_t>
unit_t parse_unit(const std::string& s) {
  Lexer lex(s);
  double value;
  boost::optional<boost::units::quantity<typename unit_t::dimension>> one = boost::make_optional(false, boost::units::quantity<typename unit_t::dimension>());
  try {
    value = lex.get_double();
    lex.get_whitespace();
    if (!lex.at_end()) {
      one = unit_t::get_unit(lex);
    }
  } catch (units_parse_exception& e) {
    std::cerr << e.what() << std::endl;
    throw boost::program_options::invalid_option_value(s);
  }
  lex.get_whitespace();
  if (!lex.at_end()) {
    std::cerr << "Extra characters at end of option" << std::endl;
    throw boost::program_options::invalid_option_value(s);
  }
  return unit_t(value, one);
}

template <typename dimension_t>
inline std::istream& operator>>(std::istream& in, Unit<dimension_t>& unit) {
  std::string s(std::istreambuf_iterator<char>(in), {});
  unit = parse_unit<Unit<dimension_t>>(s);
  return in;
}

namespace BoardSide {
enum BoardSide {
  AUTO,
  FRONT,
  BACK
};

inline std::istream& operator>>(std::istream& in, BoardSide& boardside)
{
  std::string token(std::istreambuf_iterator<char>(in), {});
  if (boost::iequals(token, "auto")) {
    boardside = BoardSide::AUTO;
  } else if (boost::iequals(token, "front")) {
    boardside = BoardSide::FRONT;
  } else if (boost::iequals(token, "back")) {
    boardside = BoardSide::BACK;
  } else {
    throw boost::program_options::invalid_option_value(token);
  }
  return in;
}

inline std::ostream& operator<<(std::ostream& out, const BoardSide& boardside)
{
  switch (boardside) {
    case BoardSide::AUTO:
      out << "auto";
      break;
    case BoardSide::FRONT:
      out << "front";
      break;
    case BoardSide::BACK:
      out << "back";
      break;
  }
  return out;
}
}; // namespace BoardSide

namespace Software {

inline std::istream& operator>>(std::istream& in, Software& software)
{
  std::string token(std::istreambuf_iterator<char>(in), {});
  if (boost::iequals(token, "Custom")) {
    software = CUSTOM;
  } else if (boost::iequals(token, "LinuxCNC")) {
    software = LINUXCNC;
  } else if (boost::iequals(token, "Mach4")) {
    software = MACH4;
  } else if (boost::iequals(token, "Mach3")) {
    software = MACH3;
  } else {
    throw boost::program_options::invalid_option_value(token);
  }
  return in;
}

inline std::ostream& operator<<(std::ostream& out, const Software& software)
{
  switch (software) {
    case CUSTOM:
      out << "custom";
      break;
    case LINUXCNC:
      out << "linuxcnc";
      break;
    case MACH4:
      out << "mach4";
      break;
    case MACH3:
      out << "mach3";
      break;
  }
  return out;
}
}; // namespace Software

namespace MillFeedDirection {
enum MillFeedDirection {
  ANY,
  CLIMB,
  CONVENTIONAL
};

inline std::istream& operator>>(std::istream& in, MillFeedDirection& millfeeddirection) {
  std::string token(std::istreambuf_iterator<char>(in), {});
  if (boost::iequals(token, "climb") ||
      boost::iequals(token, "clockwise")) {
    millfeeddirection = MillFeedDirection::CLIMB;
  } else if (boost::iequals(token, "conventional") ||
             boost::iequals(token, "anticlockwise") ||
             boost::iequals(token, "counterclockwise")) {
    millfeeddirection = MillFeedDirection::CONVENTIONAL;
  } else if (boost::iequals(token, "any")) {
    millfeeddirection = MillFeedDirection::ANY;
  } else {
    throw boost::program_options::invalid_option_value(token);
  }
  return in;
}
}; // namespace MillFeedDirection

#endif // UNITS_HPP
