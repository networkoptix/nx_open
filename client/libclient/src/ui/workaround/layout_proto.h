#ifndef QN_WORKAROUND_LAYOUT_PROTO_H
#define QN_WORKAROUND_LAYOUT_PROTO_H

#include <nx_ec/data/api_fwd.h>

class QnProtoValue;

namespace QnProto {

    template<class T>
    struct Message {
        T data;
    };

    bool deserialize(const QByteArray &value, Message<ec2::ApiLayoutData> *target);
    bool deserialize(const QnProtoValue &value, Message<ec2::ApiLayoutData> *target);
} 

#endif // QN_WORKAROUND_LAYOUT_PROTO_H
