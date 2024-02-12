// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
        registerSerializer<nx::Uuid>();
        registerSerializer<QUrl>();
        registerSerializer<nx::utils::Url>();

        registerSerializer<QnLatin1Array>();
    }
};

QnSerializerStorage<QnLexicalSerializer> *QnLexicalDetail::StorageInstance::operator()() const
{
    static QnLexicalSerializerStorage storage;
    return &storage;
}
