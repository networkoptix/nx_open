#include "json_serializer.h"

#include <utils/common/synchronized_flat_storage.h>
#include <utils/common/json.h>


// -------------------------------------------------------------------------- //
// QnJsonSerializerStorage
// -------------------------------------------------------------------------- //
class QnJsonSerializerStorage: public QnSynchronizedFlatStorage<int, QnJsonSerializer *> {
    typedef QnSynchronizedFlatStorage<int, QnJsonSerializer *> base_type;
public:
    QnJsonSerializerStorage() {
        insert<QString>();
        insert<double>();
        insert<bool>();
        insert<char>();
        insert<unsigned char>();
        insert<short>();
        insert<unsigned short>();
        insert<int>();
        insert<unsigned int>();
        insert<long>();
        insert<unsigned long>();
        insert<long long>();
        insert<unsigned long long>();

        insert<QColor>();
    }

    using base_type::insert;

    void insert(QnJsonSerializer *serializer) {
        insert(serializer->type(), serializer);
    }

    template<class T>
    void insert() {
        insert(new QJsonDetail::QnAdlJsonSerializer<T>());
    }
};

Q_GLOBAL_STATIC(QnJsonSerializerStorage, qn_jsonSerializerStorage);


// -------------------------------------------------------------------------- //
// QnJsonSerializer
// -------------------------------------------------------------------------- //
QnJsonSerializer *QnJsonSerializer::forType(int type) {
    return qn_jsonSerializerStorage()->value(type);
}

void QnJsonSerializer::registerSerializer(QnJsonSerializer *serializer) {
    qn_jsonSerializerStorage()->insert(serializer);
}
