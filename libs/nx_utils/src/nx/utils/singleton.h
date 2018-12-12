#pragma once

#include <cassert>
#include <typeinfo>

/**
 * Singleton base class that provides instance access, but does not manage
 * the object's lifetime, just like a <tt>QApplication</tt>.
 * 
 * \param Derived                       Actual type of the singleton, must be
 *                                      derived from this class.
 */
template<class Derived>
class Singleton {
public:
    static Derived *instance() {
        return s_instance;
    }

protected:
    Singleton() {
        /* Init global instance. */
        if(s_instance) {
            // TODO: #dklychov Refactor camera motion tab and enable this NX_ASSERT
            // NX_ASSERT(false);
        } else {
            s_instance = static_cast<Derived *>(this);
        }
    }

    ~Singleton() {
        if(s_instance == this)
            s_instance = nullptr;
    }

private:
    static Derived *s_instance;
};

template<class Derived>
Derived *Singleton<Derived>::s_instance = nullptr;
