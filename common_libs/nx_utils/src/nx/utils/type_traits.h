#ifndef QN_MPL_H
#define QN_MPL_H

#include <type_traits>

namespace QnTypeTraits {
    struct na {};

    template<class T>
    struct identity {
        typedef T type;
    };

    template<class T>
    struct remove_cvr:
        std::remove_cv<
            typename std::remove_reference<T>::type
        >
    {};

    struct yes_type { char dummy; };
    struct no_type { char dummy[64]; };

} // namespace mpl

#endif // QN_MPL_H
