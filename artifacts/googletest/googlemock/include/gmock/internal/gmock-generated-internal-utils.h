// This file was GENERATED by command:
//     pump.py gmock-generated-internal-utils.h.pump
// DO NOT EDIT BY HAND!!!

// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


// Google Mock - a framework for writing C++ mock classes.
//
// This file contains template meta-programming utility classes needed
// for implementing Google Mock.

// GOOGLETEST_CM0002 DO NOT DELETE

#ifndef GMOCK_INCLUDE_GMOCK_INTERNAL_GMOCK_GENERATED_INTERNAL_UTILS_H_
#define GMOCK_INCLUDE_GMOCK_INTERNAL_GMOCK_GENERATED_INTERNAL_UTILS_H_

#include "gmock/internal/gmock-port.h"

namespace testing {

template <typename T>
class Matcher;

namespace internal {

// An IgnoredValue object can be implicitly constructed from ANY value.
// This is used in implementing the IgnoreResult(a) action.
class IgnoredValue {
 public:
  // This constructor template allows any value to be implicitly
  // converted to IgnoredValue.  The object has no data member and
  // doesn't try to remember anything about the argument.  We
  // deliberately omit the 'explicit' keyword in order to allow the
  // conversion to be implicit.
  template <typename T>
  IgnoredValue(const T& /* ignored */) {}  // NOLINT(runtime/explicit)
};

// MatcherTuple<T>::type is a tuple type where each field is a Matcher
// for the corresponding field in tuple type T.
template <typename Tuple>
struct MatcherTuple;

template <>
struct MatcherTuple< ::testing::tuple<> > {
  typedef ::testing::tuple< > type;
};

template <typename A1>
struct MatcherTuple< ::testing::tuple<A1> > {
  typedef ::testing::tuple<Matcher<A1> > type;
};

template <typename A1, typename A2>
struct MatcherTuple< ::testing::tuple<A1, A2> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2> > type;
};

template <typename A1, typename A2, typename A3>
struct MatcherTuple< ::testing::tuple<A1, A2, A3> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3> > type;
};

template <typename A1, typename A2, typename A3, typename A4>
struct MatcherTuple< ::testing::tuple<A1, A2, A3, A4> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3>, Matcher<A4> >
      type;
};

template <typename A1, typename A2, typename A3, typename A4, typename A5>
struct MatcherTuple< ::testing::tuple<A1, A2, A3, A4, A5> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3>, Matcher<A4>,
                           Matcher<A5> >
      type;
};

template <typename A1, typename A2, typename A3, typename A4, typename A5,
    typename A6>
struct MatcherTuple< ::testing::tuple<A1, A2, A3, A4, A5, A6> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3>, Matcher<A4>,
                           Matcher<A5>, Matcher<A6> >
      type;
};

template <typename A1, typename A2, typename A3, typename A4, typename A5,
    typename A6, typename A7>
struct MatcherTuple< ::testing::tuple<A1, A2, A3, A4, A5, A6, A7> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3>, Matcher<A4>,
                           Matcher<A5>, Matcher<A6>, Matcher<A7> >
      type;
};

template <typename A1, typename A2, typename A3, typename A4, typename A5,
    typename A6, typename A7, typename A8>
struct MatcherTuple< ::testing::tuple<A1, A2, A3, A4, A5, A6, A7, A8> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3>, Matcher<A4>,
                           Matcher<A5>, Matcher<A6>, Matcher<A7>, Matcher<A8> >
      type;
};

template <typename A1, typename A2, typename A3, typename A4, typename A5,
    typename A6, typename A7, typename A8, typename A9>
struct MatcherTuple< ::testing::tuple<A1, A2, A3, A4, A5, A6, A7, A8, A9> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3>, Matcher<A4>,
                           Matcher<A5>, Matcher<A6>, Matcher<A7>, Matcher<A8>,
                           Matcher<A9> >
      type;
};

template <typename A1, typename A2, typename A3, typename A4, typename A5,
    typename A6, typename A7, typename A8, typename A9, typename A10>
struct MatcherTuple< ::testing::tuple<A1, A2, A3, A4, A5, A6, A7, A8, A9,
    A10> > {
  typedef ::testing::tuple<Matcher<A1>, Matcher<A2>, Matcher<A3>, Matcher<A4>,
                           Matcher<A5>, Matcher<A6>, Matcher<A7>, Matcher<A8>,
                           Matcher<A9>, Matcher<A10> >
      type;
};

// Template struct Function<F>, where F must be a function type, contains
// the following typedefs:
//
//   Result:               the function's return type.
//   ArgumentN:            the type of the N-th argument, where N starts with 1.
//   ArgumentTuple:        the tuple type consisting of all parameters of F.
//   ArgumentMatcherTuple: the tuple type consisting of Matchers for all
//                         parameters of F.
//   MakeResultVoid:       the function type obtained by substituting void
//                         for the return type of F.
//   MakeResultIgnoredValue:
//                         the function type obtained by substituting Something
//                         for the return type of F.
template <typename F>
struct Function;

template <typename R>
struct Function<R()> {
  typedef R Result;
  typedef ::testing::tuple<> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid();
  typedef IgnoredValue MakeResultIgnoredValue();
};

template <typename R, typename A1>
struct Function<R(A1)>
    : Function<R()> {
  typedef A1 Argument1;
  typedef ::testing::tuple<A1> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1);
  typedef IgnoredValue MakeResultIgnoredValue(A1);
};

template <typename R, typename A1, typename A2>
struct Function<R(A1, A2)>
    : Function<R(A1)> {
  typedef A2 Argument2;
  typedef ::testing::tuple<A1, A2> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2);
};

template <typename R, typename A1, typename A2, typename A3>
struct Function<R(A1, A2, A3)>
    : Function<R(A1, A2)> {
  typedef A3 Argument3;
  typedef ::testing::tuple<A1, A2, A3> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3);
};

template <typename R, typename A1, typename A2, typename A3, typename A4>
struct Function<R(A1, A2, A3, A4)>
    : Function<R(A1, A2, A3)> {
  typedef A4 Argument4;
  typedef ::testing::tuple<A1, A2, A3, A4> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3, A4);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3, A4);
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5>
struct Function<R(A1, A2, A3, A4, A5)>
    : Function<R(A1, A2, A3, A4)> {
  typedef A5 Argument5;
  typedef ::testing::tuple<A1, A2, A3, A4, A5> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3, A4, A5);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3, A4, A5);
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6>
struct Function<R(A1, A2, A3, A4, A5, A6)>
    : Function<R(A1, A2, A3, A4, A5)> {
  typedef A6 Argument6;
  typedef ::testing::tuple<A1, A2, A3, A4, A5, A6> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3, A4, A5, A6);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3, A4, A5, A6);
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7>
struct Function<R(A1, A2, A3, A4, A5, A6, A7)>
    : Function<R(A1, A2, A3, A4, A5, A6)> {
  typedef A7 Argument7;
  typedef ::testing::tuple<A1, A2, A3, A4, A5, A6, A7> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3, A4, A5, A6, A7);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3, A4, A5, A6, A7);
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8>
struct Function<R(A1, A2, A3, A4, A5, A6, A7, A8)>
    : Function<R(A1, A2, A3, A4, A5, A6, A7)> {
  typedef A8 Argument8;
  typedef ::testing::tuple<A1, A2, A3, A4, A5, A6, A7, A8> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3, A4, A5, A6, A7, A8);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3, A4, A5, A6, A7, A8);
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8, typename A9>
struct Function<R(A1, A2, A3, A4, A5, A6, A7, A8, A9)>
    : Function<R(A1, A2, A3, A4, A5, A6, A7, A8)> {
  typedef A9 Argument9;
  typedef ::testing::tuple<A1, A2, A3, A4, A5, A6, A7, A8, A9> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3, A4, A5, A6, A7, A8, A9);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3, A4, A5, A6, A7, A8,
      A9);
};

template <typename R, typename A1, typename A2, typename A3, typename A4,
    typename A5, typename A6, typename A7, typename A8, typename A9,
    typename A10>
struct Function<R(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10)>
    : Function<R(A1, A2, A3, A4, A5, A6, A7, A8, A9)> {
  typedef A10 Argument10;
  typedef ::testing::tuple<A1, A2, A3, A4, A5, A6, A7, A8, A9,
      A10> ArgumentTuple;
  typedef typename MatcherTuple<ArgumentTuple>::type ArgumentMatcherTuple;
  typedef void MakeResultVoid(A1, A2, A3, A4, A5, A6, A7, A8, A9, A10);
  typedef IgnoredValue MakeResultIgnoredValue(A1, A2, A3, A4, A5, A6, A7, A8,
      A9, A10);
};

}  // namespace internal

}  // namespace testing

#endif  // GMOCK_INCLUDE_GMOCK_INTERNAL_GMOCK_GENERATED_INTERNAL_UTILS_H_
