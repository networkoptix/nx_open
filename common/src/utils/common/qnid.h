#ifndef QNID_H
#define QNID_H

#include <QString>
#include <QMetaType>

class QN_EXPORT QnId
{
public:
    QnId() : m_id(0) {}
    QnId(const int value) : m_id(value) {}
    QnId(const QString &other) : m_id(other.toInt()) {}
    QnId(const char *other) : m_id(QByteArray(other).toInt()) {}
    void markUnloaded() { if (m_id > 0) m_id = -m_id; }
    void markLoaded() { if (m_id < 0) m_id = -m_id; }
    bool isLoaded() const { return m_id > 0; }
    QnId asLoaded() const { return QnId(qAbs(m_id)); }
    bool isValid() const { return m_id != 0; }
    bool isSpecial() const; // local generated ID. Id still unique
    operator uint() const { return uint(m_id); }
    QString toString() const { return QString::number(m_id); }
    int toInt() const { return m_id; }

    /**
     * Generates a new unassigned id.
     *
     * \returns                         Newly generated id.
     * 
     * \note                            This function is thread-safe.
     */
    static QnId generateSpecialId();

    QnId &operator=(const QnId &other)
    {
        m_id = other.m_id;
        return *this;
    }

private:
    int m_id;
};

Q_DECLARE_TYPEINFO(QnId, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(QnId);

#endif // QNID_H
