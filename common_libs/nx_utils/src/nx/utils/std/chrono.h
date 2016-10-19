#pragma once

#include <chrono>

#if defined(__GNUC_PREREQ) && !__GNUC_PREREQ(4,9)
    #define USE_OWN_CHRONO_LITERALS
#endif

#if defined(USE_OWN_CHRONO_LITERALS)

// This code is just copied from gcc-4.9 STL.

inline namespace literals
{
inline namespace chrono_literals
{

  namespace __select_type
  {

    using namespace __parse_int;

    template<unsigned long long _Val, typename _Dur>
  struct _Select_type
  : conditional<
      _Val <= static_cast<unsigned long long>
            (numeric_limits<typename _Dur::rep>::max()),
      _Dur, void>
  {
    static constexpr typename _Select_type::type
      value{static_cast<typename _Select_type::type>(_Val)};
  };

    template<unsigned long long _Val, typename _Dur>
  constexpr typename _Select_type<_Val, _Dur>::type
  _Select_type<_Val, _Dur>::value;

  } // __select_type

  constexpr chrono::duration<long double, ratio<3600,1>>
  operator""h(long double __hours)
  { return chrono::duration<long double, ratio<3600,1>>{__hours}; }

  template <char... _Digits>
    constexpr typename
    __select_type::_Select_type<__select_int::_Select_int<_Digits...>::value,
               chrono::hours>::type
    operator""h()
    {
  return __select_type::_Select_type<
            __select_int::_Select_int<_Digits...>::value,
            chrono::hours>::value;
    }

  constexpr chrono::duration<long double, ratio<60,1>>
  operator""min(long double __mins)
  { return chrono::duration<long double, ratio<60,1>>{__mins}; }

  template <char... _Digits>
    constexpr typename
    __select_type::_Select_type<__select_int::_Select_int<_Digits...>::value,
               chrono::minutes>::type
    operator""min()
    {
  return __select_type::_Select_type<
            __select_int::_Select_int<_Digits...>::value,
            chrono::minutes>::value;
    }

  constexpr chrono::duration<long double>
  operator""s(long double __secs)
  { return chrono::duration<long double>{__secs}; }

  template <char... _Digits>
    constexpr typename
    __select_type::_Select_type<__select_int::_Select_int<_Digits...>::value,
               chrono::seconds>::type
    operator""s()
    {
  return __select_type::_Select_type<
            __select_int::_Select_int<_Digits...>::value,
            chrono::seconds>::value;
    }

  constexpr chrono::duration<long double, milli>
  operator""ms(long double __msecs)
  { return chrono::duration<long double, milli>{__msecs}; }

  template <char... _Digits>
    constexpr typename
    __select_type::_Select_type<__select_int::_Select_int<_Digits...>::value,
               chrono::milliseconds>::type
    operator""ms()
    {
  return __select_type::_Select_type<
            __select_int::_Select_int<_Digits...>::value,
            chrono::milliseconds>::value;
    }

  constexpr chrono::duration<long double, micro>
  operator""us(long double __usecs)
  { return chrono::duration<long double, micro>{__usecs}; }

  template <char... _Digits>
    constexpr typename
    __select_type::_Select_type<__select_int::_Select_int<_Digits...>::value,
               chrono::microseconds>::type
    operator""us()
    {
  return __select_type::_Select_type<
            __select_int::_Select_int<_Digits...>::value,
            chrono::microseconds>::value;
    }

  constexpr chrono::duration<long double, nano>
  operator""ns(long double __nsecs)
  { return chrono::duration<long double, nano>{__nsecs}; }

  template <char... _Digits>
    constexpr typename
    __select_type::_Select_type<__select_int::_Select_int<_Digits...>::value,
               chrono::nanoseconds>::type
    operator""ns()
    {
  return __select_type::_Select_type<
            __select_int::_Select_int<_Digits...>::value,
            chrono::nanoseconds>::value;
    }

} // inline namespace chrono_literals
} // inline namespace literals

namespace chrono
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

using namespace literals::chrono_literals;

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace chrono

#endif //defined(USE_OWN_CHRONO_LITERALS)
