#!/usr/bin/env python
#
# Copyright 2009 Neal Norwitz All Rights Reserved.
# Portions Copyright 2009 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for gmock.scripts.generator.cpp.gmock_class."""

import os
import sys
import unittest

# Allow the cpp imports below to work when run as a standalone script.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from cpp import ast
from cpp import gmock_class


class TestCase(unittest.TestCase):
  """Helper class that adds assert methods."""

  @staticmethod
  def StripLeadingWhitespace(lines):
    """Strip leading whitespace in each line in 'lines'."""
    return '\n'.join([s.lstrip() for s in lines.split('\n')])

  def assertEqualIgnoreLeadingWhitespace(self, expected_lines, lines):
    """Specialized assert that ignores the indent level."""
    self.assertEqual(expected_lines, self.StripLeadingWhitespace(lines))


class GenerateMethodsTest(TestCase):

  @staticmethod
  def GenerateMethodSource(cpp_source):
    """Convert C++ source to Google Mock output source lines."""
    method_source_lines = []
    # <test> is a pseudo-filename, it is not read or written.
    builder = ast.BuilderFromSource(cpp_source, '<test>')
    ast_list = list(builder.Generate())
    gmock_class._GenerateMethods(method_source_lines, cpp_source, ast_list[0])
    return '\n'.join(method_source_lines)

  def testSimpleMethod(self):
    source = """
class Foo {
 public:
  virtual int Bar();
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testSimpleConstructorsAndDestructor(self):
    source = """
class Foo {
 public:
  Foo();
  Foo(int x);
  Foo(const Foo& f);
  Foo(Foo&& f);
  ~Foo();
  virtual int Bar() = 0;
};
"""
    # The constructors and destructor should be ignored.
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testVirtualDestructor(self):
    source = """
class Foo {
 public:
  virtual ~Foo();
  virtual int Bar() = 0;
};
"""
    # The destructor should be ignored.
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testExplicitlyDefaultedConstructorsAndDestructor(self):
    source = """
class Foo {
 public:
  Foo() = default;
  Foo(const Foo& f) = default;
  Foo(Foo&& f) = default;
  ~Foo() = default;
  virtual int Bar() = 0;
};
"""
    # The constructors and destructor should be ignored.
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testExplicitlyDeletedConstructorsAndDestructor(self):
    source = """
class Foo {
 public:
  Foo() = delete;
  Foo(const Foo& f) = delete;
  Foo(Foo&& f) = delete;
  ~Foo() = delete;
  virtual int Bar() = 0;
};
"""
    # The constructors and destructor should be ignored.
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testSimpleOverrideMethod(self):
    source = """
class Foo {
 public:
  int Bar() override;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testSimpleConstMethod(self):
    source = """
class Foo {
 public:
  virtual void Bar(bool flag) const;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(void, Bar, (bool flag), (const, override));',
        self.GenerateMethodSource(source))

  def testExplicitVoid(self):
    source = """
class Foo {
 public:
  virtual int Bar(void);
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testStrangeNewlineInParameter(self):
    source = """
class Foo {
 public:
  virtual void Bar(int
a) = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(void, Bar, (int a), (override));',
        self.GenerateMethodSource(source))

  def testDefaultParameters(self):
    source = """
class Foo {
 public:
  virtual void Bar(int a, char c = 'x') = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(void, Bar, (int a, char c), (override));',
        self.GenerateMethodSource(source))

  def testMultipleDefaultParameters(self):
    source = """
class Foo {
 public:
  virtual void Bar(
        int a = 42, 
        char c = 'x', 
        const int* const p = nullptr, 
        const std::string& s = "42",
        char tab[] = {'4','2'},
        int const *& rp = aDefaultPointer) = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(void, Bar, '
        '(int a, char c, const int* const p, const std::string& s, char tab[], int const *& rp), '
        '(override));', self.GenerateMethodSource(source))

  def testMultipleSingleLineDefaultParameters(self):
    source = """
class Foo {
 public:
  virtual void Bar(int a = 42, int b = 43, int c = 44) = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(void, Bar, (int a, int b, int c), (override));',
        self.GenerateMethodSource(source))

  def testConstDefaultParameter(self):
    source = """
class Test {
 public:
  virtual bool Bar(const int test_arg = 42) = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(bool, Bar, (const int test_arg), (override));',
        self.GenerateMethodSource(source))

  def testConstRefDefaultParameter(self):
    source = """
class Test {
 public:
  virtual bool Bar(const std::string& test_arg = "42" ) = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(bool, Bar, (const std::string& test_arg), (override));',
        self.GenerateMethodSource(source))

  def testRemovesCommentsWhenDefaultsArePresent(self):
    source = """
class Foo {
 public:
  virtual void Bar(int a = 42 /* a comment */,
                   char /* other comment */ c= 'x') = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(void, Bar, (int a, char c), (override));',
        self.GenerateMethodSource(source))

  def testDoubleSlashCommentsInParameterListAreRemoved(self):
    source = """
class Foo {
 public:
  virtual void Bar(int a,  // inline comments should be elided.
                   int b   // inline comments should be elided.
                   ) const = 0;
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(void, Bar, (int a, int b), (const, override));',
        self.GenerateMethodSource(source))

  def testCStyleCommentsInParameterListAreNotRemoved(self):
    # NOTE(nnorwitz): I'm not sure if it's the best behavior to keep these
    # comments.  Also note that C style comments after the last parameter
    # are still elided.
    source = """
class Foo {
 public:
  virtual const string& Bar(int /* keeper */, int b);
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(const string&, Bar, (int, int b), (override));',
        self.GenerateMethodSource(source))

  def testArgsOfTemplateTypes(self):
    source = """
class Foo {
 public:
  virtual int Bar(const vector<int>& v, map<int, string>* output);
};"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (const vector<int>& v, (map<int, string>* output)), (override));',
        self.GenerateMethodSource(source))

  def testReturnTypeWithOneTemplateArg(self):
    source = """
class Foo {
 public:
  virtual vector<int>* Bar(int n);
};"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(vector<int>*, Bar, (int n), (override));',
        self.GenerateMethodSource(source))

  def testReturnTypeWithManyTemplateArgs(self):
    source = """
class Foo {
 public:
  virtual map<int, string> Bar();
};"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD((map<int, string>), Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testSimpleMethodInTemplatedClass(self):
    source = """
template<class T>
class Foo {
 public:
  virtual int Bar();
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (), (override));',
        self.GenerateMethodSource(source))

  def testPointerArgWithoutNames(self):
    source = """
class Foo {
  virtual int Bar(C*);
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (C*), (override));',
        self.GenerateMethodSource(source))

  def testReferenceArgWithoutNames(self):
    source = """
class Foo {
  virtual int Bar(C&);
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (C&), (override));',
        self.GenerateMethodSource(source))

  def testArrayArgWithoutNames(self):
    source = """
class Foo {
  virtual int Bar(C[]);
};
"""
    self.assertEqualIgnoreLeadingWhitespace(
        'MOCK_METHOD(int, Bar, (C[]), (override));',
        self.GenerateMethodSource(source))


class GenerateMocksTest(TestCase):

  @staticmethod
  def GenerateMocks(cpp_source):
    """Convert C++ source to complete Google Mock output source."""
    # <test> is a pseudo-filename, it is not read or written.
    filename = '<test>'
    builder = ast.BuilderFromSource(cpp_source, filename)
    ast_list = list(builder.Generate())
    lines = gmock_class._GenerateMocks(filename, cpp_source, ast_list, None)
    return '\n'.join(lines)

  def testNamespaces(self):
    source = """
namespace Foo {
namespace Bar { class Forward; }
namespace Baz::Qux {

class Test {
 public:
  virtual void Foo();
};

}  // namespace Baz::Qux
}  // namespace Foo
"""
    expected = """\
namespace Foo {
namespace Baz::Qux {

class MockTest : public Test {
public:
MOCK_METHOD(void, Foo, (), (override));
};

}  // namespace Baz::Qux
}  // namespace Foo
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testClassWithStorageSpecifierMacro(self):
    source = """
class STORAGE_SPECIFIER Test {
 public:
  virtual void Foo();
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(void, Foo, (), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testTemplatedForwardDeclaration(self):
    source = """
template <class T> class Forward;  // Forward declaration should be ignored.
class Test {
 public:
  virtual void Foo();
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(void, Foo, (), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testTemplatedClass(self):
    source = """
template <typename S, typename T>
class Test {
 public:
  virtual void Foo();
};
"""
    expected = """\
template <typename S, typename T>
class MockTest : public Test<S, T> {
public:
MOCK_METHOD(void, Foo, (), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testTemplateInATemplateTypedef(self):
    source = """
class Test {
 public:
  typedef std::vector<std::list<int>> FooType;
  virtual void Bar(const FooType& test_arg);
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(void, Bar, (const FooType& test_arg), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testTemplatedClassWithTemplatedArguments(self):
    source = """
template <typename S, typename T, typename U, typename V, typename W>
class Test {
 public:
  virtual U Foo(T some_arg);
};
"""
    expected = """\
template <typename S, typename T, typename U, typename V, typename W>
class MockTest : public Test<S, T, U, V, W> {
public:
MOCK_METHOD(U, Foo, (T some_arg), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testTemplateInATemplateTypedefWithComma(self):
    source = """
class Test {
 public:
  typedef std::function<void(
      const vector<std::list<int>>&, int> FooType;
  virtual void Bar(const FooType& test_arg);
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(void, Bar, (const FooType& test_arg), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testParenthesizedCommaInArg(self):
    source = """
class Test {
 public:
   virtual void Bar(std::function<void(int, int)> f);
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(void, Bar, (std::function<void(int, int)> f), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testEnumType(self):
    source = """
class Test {
 public:
  enum Bar {
    BAZ, QUX, QUUX, QUUUX
  };
  virtual void Foo();
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(void, Foo, (), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testEnumClassType(self):
    source = """
class Test {
 public:
  enum class Bar {
    BAZ, QUX, QUUX, QUUUX
  };
  virtual void Foo();
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(void, Foo, (), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))

  def testStdFunction(self):
    source = """
class Test {
 public:
  Test(std::function<int(std::string)> foo) : foo_(foo) {}

  virtual std::function<int(std::string)> foo();

 private:
  std::function<int(std::string)> foo_;
};
"""
    expected = """\
class MockTest : public Test {
public:
MOCK_METHOD(std::function<int (std::string)>, foo, (), (override));
};
"""
    self.assertEqualIgnoreLeadingWhitespace(expected,
                                            self.GenerateMocks(source))


if __name__ == '__main__':
  unittest.main()
