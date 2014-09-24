#ifndef QN_SERIALIZATION_COLLECTION_FWD_H
#define QN_SERIALIZATION_COLLECTION_FWD_H

/**
 * \file
 * 
 * This file contains forward-declarations for common collection types that are
 * handled by the serialization framework.
 * 
 * They are here so that there is no need to explicitly enumerate them every
 * time a new format is added.
 * 
 * Note that STL containers cannot be safely forward-declared (standard says
 * it's undefined behavior to define anything in STL namespace), so these
 * will still have to be included manually.
 */

template<class T> class QSet;
template<class T> class QList;
template<class T> class QLinkedList;
template<class T> class QVector;
template<class T, int N> class QVarLengthArray;
template<class Key, class T> class QMap;
template<class Key, class T> class QHash;

#endif // QN_SERIALIZATION_COLLECTION_FWD_H
