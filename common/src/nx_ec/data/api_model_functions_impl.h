#ifndef QN_API_MODEL_FUNCTIONS_H
#define QN_API_MODEL_FUNCTIONS_H

// this file is not to be directly included in target modules
// DO NOT INCLUDE THIS FILE UNLESS YOU KNOW WHAT ARE YOU DOING

#include "api_fwd.h"

#include <boost/preprocessor/seq/to_tuple.hpp>

#include <utils/common/model_functions.h>

/* Some fields are not meant to be bound or fetched. */
template<class T, class Allocator>
inline void serialize_field(const std::vector<T, Allocator> &, QVariant *) { return; }
template<class Key, class T, class Predicate, class Allocator>
inline void serialize_field(const std::map<Key, T, Predicate, Allocator> &, QVariant *) { return; }
template<class Key, class T>
inline void serialize_field(const QMap<Key, T> &, QVariant *) { return; }
template<class T>
inline void serialize_field(const QList<T> &, QVariant *) { return; }

inline void serialize_field(const ec2::ApiPeerData &, QVariant *) { return; }
inline void serialize_field(const ec2::ApiRuntimeData &, QVariant *) { return; }
inline void serialize_field(const ec2::ApiPeerAliveData &, QVariant *) { return; }
inline void serialize_field(const ec2::QnTranState &, QVariant *) { return; }

template<class T, class Allocator>
inline void deserialize_field(const QVariant &, std::vector<T, Allocator> *) { return; }
template<class Key, class T, class Predicate, class Allocator>
inline void deserialize_field(const QVariant &, std::map<Key, T, Predicate, Allocator> *) { return; }
template<class Key, class T>
inline void deserialize_field(const QVariant &, QMap<Key, T> *) { return; }
template<class T>
inline void deserialize_field(const QVariant &, QList<T> *) { return; }

inline void deserialize_field(const QVariant &, ec2::ApiPeerData *) { return; }
inline void deserialize_field(const QVariant &, ec2::ApiRuntimeData *) { return; }
inline void deserialize_field(const QVariant &, ec2::ApiPeerAliveData *) { return; }
inline void deserialize_field(const QVariant &, ec2::QnTranState *) { return; }

#endif //QN_API_MODEL_FUNCTIONS_H
