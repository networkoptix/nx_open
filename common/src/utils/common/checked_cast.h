#ifndef QN_CORE_CHECKED_CAST_H
#define QN_CORE_CHECKED_CAST_H

#include <cassert>
#include <QSharedPointer>

template<class Target, class Source>
Target checked_cast(Source *source) {
    // static_assert(boost::is_pointer<Target>::value, "Target type must be a pointer");
#ifndef NDEBUG
    Target result = dynamic_cast<Target>(source);
    assert(source == NULL || result != NULL);
    return result;
#else
    return static_cast<Target>(source);
#endif // NDEBUG
}

template<class Target, class Source>
Target checked_cast(Source &source) {
    // static_assert(boost::is_reference<Target>::value, "Target type must be a reference");
#ifndef NDEBUG
    return dynamic_cast<Target>(source);
#else
    return static_cast<Target>(source);
#endif // NDEBUG
}

template<class Target, class Source>
QSharedPointer<Target> checked_cast(const QSharedPointer<Source> &source) {
#ifndef NDEBUG
    QSharedPointer<Target> result = source.template dynamicCast<Target>();
    assert(source.isNull() || !result.isNull());
    return result;
#else
    return source.staticCast<Target>();
#endif // NDEBUG
}

/**
 * This definition is here to get at least some syntax highlighting for checked_cast.
 */
#define checked_cast checked_cast

#endif // QN_CORE_CHECKED_CAST_H

