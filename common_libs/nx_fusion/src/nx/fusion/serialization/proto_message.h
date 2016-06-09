#ifndef QN_PROTO_MESSAGE_H
#define QN_PROTO_MESSAGE_H

#include <QtCore/QVector>

#include "proto_value.h"
#include "proto_record.h"
#include "proto_parse_error.h"


class QnProtoMessage {
public:
    typedef QVector<QnProtoRecord>::const_iterator const_iterator;
    typedef QVector<QnProtoRecord>::iterator iterator;

    QnProtoMessage() {}

    static QnProtoMessage fromProto(const QByteArray &proto, QnProtoParseError *error = NULL);

    int size() const {
        return m_records.size();
    }

    bool isEmpty() const {
        return m_records.isEmpty();
    }

    bool empty() const {
        return m_records.empty();
    }

    iterator begin() { return m_records.begin(); }
    const_iterator begin() const { return m_records.begin(); }
    const_iterator constBegin() const { return m_records.constBegin(); }
    
    iterator end() { return m_records.end(); }
    const_iterator end() const { return m_records.end(); }
    const_iterator constEnd() const { return m_records.constEnd(); }

    iterator erase(iterator pos) { return m_records.erase(pos); }
    iterator insert(iterator before, const QnProtoRecord &value) { return m_records.insert(before, value); }
    
    void push_back(const QnProtoRecord &value) { m_records.push_back(value); }
    void pop_back() { m_records.pop_back(); }

private:
    QVector<QnProtoRecord> m_records;
};




#endif // QN_PROTO_MESSAGE_H
