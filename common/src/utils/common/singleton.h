#ifndef QN_SINGLETON_H
#define QN_SINGLETON_H

#include <typeinfo>

#include "warnings.h"

template<class Derived>
class QnSingleton {
public:
    static Derived *instance() {
        return s_instance;
    }

protected:
    QnSingleton() {
        /* Init global instance. */
        if(s_instance) {
            qnWarning("Instance of %1 already exists.", typeid(Derived).name());
        } else {
            s_instance = static_cast<Derived *>(this);
        }
    }

    ~QnSingleton() {
        if(s_instance == this)
            s_instance = NULL;
    }

private:
    static Derived *s_instance;
};

template<class Derived>
Derived *QnSingleton<Derived>::s_instance = NULL;


#endif // QN_SINGLETON_H
