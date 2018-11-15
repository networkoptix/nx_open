#ifndef QN_INVOCATION_EVENT_H
#define QN_INVOCATION_EVENT_H

#include <QtCore/QEvent>
#include <QtCore/QVariant>

namespace QnEvent {
    /** Event type for invocation. */
    static const QEvent::Type Invocation = static_cast<QEvent::Type>(QEvent::User + 0x3896);
}

/**
 * Universal custom event.
 * 
 * Can be used to pass additional data to the receiver.
 */
class QnInvocationEvent: public QEvent {
public:
    QnInvocationEvent(int id = 0, const QVariant &data = QVariant()): 
        QEvent(QnEvent::Invocation),
        m_id(id),
        m_data(data)
    {}

    int id() const {
        return m_id;
    }

    const QVariant &data() const {
        return m_data;
    }
    
private:
    int m_id;
    QVariant m_data;
};

#endif // QN_INVOCATION_EVENT_H
