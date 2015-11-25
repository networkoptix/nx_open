#ifndef QN_SINGLETON_H
#define QN_SINGLETON_H

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
            assert(false);
        } else {
            s_instance = static_cast<Derived *>(this);
        }
    }

    ~Singleton() {
        if(s_instance == this)
            s_instance = NULL;
    }

private:
    static Derived *s_instance;
};

template<class Derived>
Derived *Singleton<Derived>::s_instance = NULL;

#endif // QN_SINGLETON_H
