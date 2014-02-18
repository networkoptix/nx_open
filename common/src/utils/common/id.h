#ifndef QN_ID_H
#define QN_ID_H

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QUuid>

typedef QUuid QnId;

/*
class QnId {
public:
    QnId() : m_id() {}
    QnId(const QString& val) : m_id(val) {}
    QnId(const QByteArray& val) : m_id(val) {}
    QnId(const QUuid& val) : m_id(val) {}
    QnId(const QnId &other) : m_id(other.m_id) {}

    operator QString() { return m_id.toString(); }
    QString toString() const { return m_id.toString(); }

    bool isValid() const { return !m_id.isNull(); }
    QByteArray serialize() const { return m_id.toRfc4122(); }

    static QnId createId() { return QnId(QUuid::createUuid()); }
    QnId &operator=(const QnId &other) { m_id = other.m_id; return *this; }
    bool operator==(const QnId &other) const { return m_id == other.m_id; }
    bool operator!=(const QnId &other) const { return m_id != other.m_id; }
    bool operator<(const QnId &other) const { return m_id < other.m_id; }

private:
    friend uint qHash(const QnId &t);

    QUuid m_id;
};

inline uint qHash(const QnId &t) { return qHash(t.m_id); }

Q_DECLARE_TYPEINFO(QnId, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(QnId);

QN_DECLARE_FUNCTIONS_FOR_TYPES((QnId), (json))
*/

#endif // QN_ID_H
