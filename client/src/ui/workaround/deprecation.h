#ifndef QN_DEPRECATION_H
#define QN_DEPRECATION_H

/**
 * @file
 * 
 * Hacks to prevent the usage of some badly designed libraries and library features.
 */

#ifdef __cplusplus

#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>

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
#include <QtCore/QMapIterator>
#define QListIterator           qt_java_style_iterators_are_forbidden
#define QLinkedListIterator     qt_java_style_iterators_are_forbidden
//#define QHashIterator           qt_java_style_iterators_are_forbidden
#define QMutableHashIterator    qt_java_style_iterators_are_forbidden
#define QVectorIterator         qt_java_style_iterators_are_forbidden
//#define QMapIterator            qt_java_style_iterators_are_forbidden // TODO: #Elric


/* Prevent the usage of local 8-bit encodings for QString. Use toLatin1 instead. */
//#define toLocal8Bit                                                             \
//    BOOST_PP_IF(                                                                \
//        BOOST_PP_IS_EMPTY(QTSERVICE_H), /* Make sure QtService compiles. */     \
//        toLocal8Bit,                                                            \
//        toLocal8Bit_is_forbidden                                                \
//    ) 

/* qPrintable uses toLocal8Bit, so we have to redefine it. Not a big loss. */
#undef qPrintable
#define qPrintable(string) QString(string).toLatin1().constData()


/* Prevent the usage of Q_GLOBAL_STATIC_WITH_INITIALIZER as it is not thread-safe.
 * See http://lists.qt-project.org/pipermail/development/2012-March/002636.html. */
#undef Q_GLOBAL_STATIC_WITH_INITIALIZER
#define Q_GLOBAL_STATIC_WITH_INITIALIZER Q_GLOBAL_STATIC_WITH_INITIALIZER___IS_DEPRECATED 


/* Prevent usage of custom file dialogs on mac os due to appstore limitations  */
#ifdef Q_OS_MAC
#   define DontUseNativeDialog YOU_SHOULD_ALWAYS_USE_NATIVE_DIALOGS_ON_MAC_OS
#endif

#endif // __cplusplus

#endif // QN_DEPRECATION_H
