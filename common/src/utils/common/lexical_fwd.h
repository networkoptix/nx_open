#ifndef QN_LEXICAL_FWD_H
#define QN_LEXICAL_FWD_H

#include <QtCore/QString>

/**
 * \param TYPE                          Type to declare lexical (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_DECLARE_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)      \
__VA_ARGS__ void serialize(const TYPE &value, QString *target);                 \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target);

#endif // QN_LEXICAL_FWD_H
