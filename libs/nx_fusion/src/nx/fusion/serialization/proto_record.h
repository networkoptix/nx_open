#ifndef QN_PROTO_RECORD_H
#define QN_PROTO_RECORD_H

#include "proto_value.h"

class QnProtoRecord {
public:
    QnProtoRecord(): m_index(0) {}
    QnProtoRecord(int index, const QnProtoValue &value): m_index(index), m_value(value) {}

    int index() const {
        return m_index;
    }

    const QnProtoValue &value() const {
        return m_value;
    }

private:
    int m_index;
    QnProtoValue m_value;
};

#endif // QN_PROTO_RECORD_H
