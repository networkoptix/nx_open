// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_LEXICAL_FWD_H
#define QN_SERIALIZATION_LEXICAL_FWD_H

class QString;

/**
 * \param TYPE                          Type to declare lexical (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * NOTE:                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_FUSION_DECLARE_FUNCTIONS_lexical(TYPE, ... /* PREFIX */)             \
__VA_ARGS__ void serialize(const TYPE &value, QString *target);                 \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target);

#endif // QN_SERIALIZATION_LEXICAL_FWD_H
