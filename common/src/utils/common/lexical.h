#ifndef QN_LEXICAL_H
#define QN_LEXICAL_H

#include <cassert>

#include <QtCore/QString>

namespace QnLexicalDetail {
    class QStringWrapper {
    public:
        QStringWrapper(const QString &string): m_string(string) {}

        operator const QString &() const {
            return m_string;
        }

    private:
        const QString &m_string;
    };

    template<class T>
    void serialize_value(const T &value, QString *target) {
        serialize(value, target); /* That's the place where ADL kicks in. */
    }

    template<class T>
    bool deserialize_value(const QString &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a string into a wrapper so that
         * ADL would find only overloads with QString as the first parameter. 
         * Otherwise other overloads could be discovered. */
        return deserialize(QStringWrapper(value), target);
    }

} // namespace QnLexicalDetail


namespace QnLexical {
    template<class T>
    void serialize(const T &value, QString *target) {
        assert(target);

        QnLexicalDetail::serialize_value(value, target);
    }

    template<class T>
    bool deserialize(const QString &value, T *target) {
        assert(target);

        return QnLexicalDetail::deserialize_value(value, target);
    }

    template<class T>
    QString serialized(const T &value) {
        QString result;
        QnLexical::serialize(value, &result);
        return result;
    }

    template<class T>
    T deserialized(const QString &value, bool *success = NULL) {
        T target;
        bool result = QnLexical::deserialize(value, &target);
        if (success)
            *success = result;
        return target;
    }

} // namespace QnLexical


/**
 * \param TYPE                          Type to declare lexical (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_DECLARE_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)      \
__VA_ARGS__ void serialize(const TYPE &value, QString *target);                 \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target);


#include "lexical_functions.h"

#endif // QN_LEXICAL_H
