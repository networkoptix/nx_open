#ifndef QN_DEPRECATION_H
#define QN_DEPRECATION_H

/**
 * @file
 * 
 * Hacks to prevent the usage of some badly designed libraries and library features.
 */

#ifdef __cplusplus

/* Prevent the usage of boost pointer containers. */
namespace boost {
    struct ptr_vector {};
    struct ptr_list {};
    struct ptr_deque {};
    struct ptr_array {};
    struct ptr_set {};
    struct ptr_multiset {};
    struct ptr_map {};
    struct ptr_multimap {};
}


/* Prevent the usage of Qt Java-style iterators. */
#include <QtCore/QListIterator>
#include <QtCore/QLinkedListIterator>
#include <QtCore/QHashIterator>
#include <QtCore/QMutableHashIterator>
#include <QtCore/QVectorIterator>
#define QListIterator           qt_java_style_iterators_are_forbidden
#define QLinkedListIterator     qt_java_style_iterators_are_forbidden
#define QHashIterator           qt_java_style_iterators_are_forbidden
#define QMutableHashIterator    qt_java_style_iterators_are_forbidden
#define QVectorIterator         qt_java_style_iterators_are_forbidden

#endif // __cplusplus

#endif // QN_DEPRECATION_H
