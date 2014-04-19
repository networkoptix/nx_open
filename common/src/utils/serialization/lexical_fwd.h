#ifndef QN_SERIALIZATION_LEXICAL_FWD_H
#define QN_SERIALIZATION_LEXICAL_FWD_H

#include <utils/fusion/fusion_fwd.h>

class QString;

/**
 * \param TYPE                          Type to declare lexical (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_FUSION_DECLARE_FUNCTIONS_lexical(TYPE, ... /* PREFIX */)             \
__VA_ARGS__ void serialize(const TYPE &value, QString *target);                 \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target);

#endif // QN_SERIALIZATION_LEXICAL_FWD_H
