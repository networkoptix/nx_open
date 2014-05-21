#ifndef QN_SERIALIZATION_LEXICAL_FWD_H
#define QN_SERIALIZATION_LEXICAL_FWD_H

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


/**
 * \param TYPE                          Type to declare as numerically serializable.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                Even though this macro only generates function
 *                                      declarations, there is no need to generate
 *                                      function definitions for the provided type.
 *                                      Appropriate functions will be picked up automatically.
 */
#define QN_FUSION_DECLARE_FUNCTIONS_numeric(TYPE, ... /* PREFIX */)             \
__VA_ARGS__ bool lexical_numeric_check(const TYPE *);


#endif // QN_SERIALIZATION_LEXICAL_FWD_H
