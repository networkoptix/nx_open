#ifndef QN_WORKAROUND_LAYOUT_PROTO_H
#define QN_WORKAROUND_LAYOUT_PROTO_H

#include <nx_ec/data/api_fwd.h>

class QnProtoValue;

namespace QnProto {

    template<class T>
    struct Message {
        T data;
    };

    bool deserialize(const QByteArray &value, Message<nx::vms::api::LayoutData> *target);
    bool deserialize(const QnProtoValue &value, Message<nx::vms::api::LayoutData> *target);
}

#endif // QN_WORKAROUND_LAYOUT_PROTO_H
