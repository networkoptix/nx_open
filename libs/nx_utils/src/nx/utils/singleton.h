// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <typeinfo>

class NX_UTILS_API SingletonBase
{
protected:
    void printInstantiationError(const std::type_info& typeInfo);
};

/**
 * Singleton base class that provides instance access, but does not manage the object's lifetime,
 * just like a <tt>QApplication</tt>. Derived classes must instantiate single s_instance member in
 * an appropriate translation unit. This member failed to be static inline as GCC and Clang produce
 * its duplicates in case of hidden visibility shared libraries.
 *
 * \param Derived                       Actual type of the singleton, must be
 *                                      derived from this class.
 */
template<class Derived>
class Singleton: public SingletonBase
{
public:
    static Derived* instance()
    {
        return s_instance;
    }

protected:
    Singleton()
    {
        // Init global instance.
        if (s_instance)
            printInstantiationError(typeid(Derived));
        else
            s_instance = static_cast<Derived*>(this);
    }

    ~Singleton()
    {
        if (s_instance == this)
            s_instance = nullptr;
    }

private:
    static NX_FORCE_EXPORT Derived* s_instance;
};
