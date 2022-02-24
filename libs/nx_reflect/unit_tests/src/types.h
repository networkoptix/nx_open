// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::reflect::test {

class Stringable
{
public:
    Stringable& operator=(const std::string& value)
    {
        m_value = value;
        return *this;
    }

    std::string toStdString() const { return m_value; }

    static Stringable fromStdString(const std::string& str)
    {
        Stringable result;
        result = str;
        return result;
    }

private:
    std::string m_value;
};

struct C
{
    std::string str;

    bool operator==(const C& rhs) const
    {
        return str == rhs.str;
    }
};

std::string toString(const C& c);
bool fromString(const std::string& str, C* c);

namespace BB {

class B
{
public:
    int num = 0;
    std::string str;
    Stringable text;
    std::vector<int> numbers;
    std::vector<std::string> strings;

    bool operator==(const B& rhs) const
    {
        return num == rhs.num
            && str == rhs.str
            && numbers == rhs.numbers
            && strings == rhs.strings;
    }
};

NX_REFLECTION_INSTRUMENT(B, (num)(str)(text)(numbers)(strings))

} // namespace B

class A
{
public:
    int num = 0;
    std::string str;
    BB::B b;
    std::vector<BB::B> bs;
    C c;
    std::map<std::string, std::string> keyToValue;

    bool operator==(const A& rhs) const
    {
        return num == rhs.num
            && str == rhs.str
            && b == rhs.b
            && bs == rhs.bs
            && c == rhs.c
            && keyToValue == rhs.keyToValue;
    }
};

NX_REFLECTION_INSTRUMENT(A, (num)(str)(b)(bs)(c)(keyToValue))

class D:
    public A
{
public:
    int ddd = 0;

    bool operator==(const D& rhs) const
    {
        return this->A::operator==(rhs)
            && ddd == rhs.ddd;
    }
};

// Repeating parent fields explicitly.
NX_REFLECTION_INSTRUMENT(D, (num)(str)(b)(bs)(c)(keyToValue)(ddd))

class E:
    public D
{
public:
    using base_type = D;

    int e = 0;

    bool operator==(const E& rhs) const
    {
        return this->D::operator==(rhs)
            && e == rhs.e;
    }
};

// Using E::base_type to include parent fields.
NX_REFLECTION_INSTRUMENT(E, (e))

} // namespace nx::reflect::test
