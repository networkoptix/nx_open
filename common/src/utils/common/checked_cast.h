#ifndef QN_CORE_CHECKED_CAST_H
#define QN_CORE_CHECKED_CAST_H

#include <cassert>
#include <QSharedPointer>

template<class Target, class Source>
Target checked_cast(Source *source) {
    // static_assert(boost::is_pointer<Target>::value, "Target type must be a pointer");
#ifdef _DEBUG
    Target result = dynamic_cast<Target>(source);
    assert(source == NULL || result != NULL);
    return result;
#else
    return static_cast<Target>(source);
#endif // _DEBUG
}

template<class Target, class Source>
Target checked_cast(Source &source) {
    // static_assert(boost::is_reference<Target>::value, "Target type must be a reference");
#ifdef _DEBUG
    return dynamic_cast<Target>(source);
#else
    return static_cast<Target>(source);
#endif // _DEBUG
}

template<class Target, class Source>
QSharedPointer<Target> checked_cast(const QSharedPointer<Source> &source) {
#ifdef _DEBUG
    QSharedPointer<Target> result = source.template dynamicCast<Target>();
    assert(source.isNull() || !result.isNull());
    return result;
#else
    return source.template staticCast<Target>();
#endif // _DEBUG
}

/**
 * This definition is here to get at least some syntax highlighting for checked_cast.
 */
#define checked_cast checked_cast

#endif // QN_CORE_CHECKED_CAST_H

