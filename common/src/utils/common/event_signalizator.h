#ifndef QN_EVENT_SIGNALIZATOR_H
#define QN_EVENT_SIGNALIZATOR_H

#include <QObject>
#include <QEvent>

class QnEventSignalizator: public QObject {
    Q_OBJECT;

public:
    QnEventSignalizator(QObject *parent = NULL): QObject(parent) {}

    virtual bool eventFilter(QObject *watched, QEvent *event) override {
        if(isActivating(event))
            emit activated(watched, event);
        
        return QObject::eventFilter(watched, event);
    }

signals:
    void activated(QObject *object, QEvent *event);

protected:
    virtual bool isActivating(QEvent *event) const = 0;
};


class QnSingleEventSignalizator: public QnEventSignalizator {
    Q_OBJECT;

public:
    QnSingleEventSignalizator(QObject *parent = NULL): QnEventSignalizator(parent), m_type(QEvent::None) {}

    void setEventType(QEvent::Type type) {
        m_type = type;
    }

    QEvent::Type eventType() const {
        return m_type;
    }

protected:
    virtual bool isActivating(QEvent *event) const override {
        return event->type() == m_type;
    }

private:
    QEvent::Type m_type;
};


#endif // QN_EVENT_SIGNALIZATOR_H
