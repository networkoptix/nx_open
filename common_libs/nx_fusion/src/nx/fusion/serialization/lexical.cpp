#include "lexical.h"
#include "lexical_functions.h"

#include <nx/fusion/serialization/chrono_metatypes.h>

#include <nx/utils/url.h>

class QnLexicalSerializerStorage: public QnSerializerStorage<QnLexicalSerializer> {
public:
    QnLexicalSerializerStorage() {
        registerSerializer<bool>();
        registerSerializer<char>();
        registerSerializer<signed char>();
        registerSerializer<unsigned char>();
        registerSerializer<short>();
        registerSerializer<unsigned short>();
        registerSerializer<int>();
        registerSerializer<unsigned int>();
        registerSerializer<long>();
        registerSerializer<unsigned long>();
        registerSerializer<long long>();
        registerSerializer<unsigned long long>();
        registerSerializer<float>();
        registerSerializer<double>();

        registerSerializer<std::chrono::milliseconds>();

        registerSerializer<QString>();

        registerSerializer<QColor>();
        registerSerializer<QnUuid>();
        registerSerializer<QUrl>();
        registerSerializer<nx::utils::Url>();

        registerSerializer<QnLatin1Array>();
    }
};

Q_GLOBAL_STATIC(QnLexicalSerializerStorage, qn_lexicalSerializerStorage_instance)

QnSerializerStorage<QnLexicalSerializer> *QnLexicalDetail::StorageInstance::operator()() const {
    return qn_lexicalSerializerStorage_instance();
}
