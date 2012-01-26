#ifndef QN_EVENT_SIGNALIZER_H
#define QN_EVENT_SIGNALIZER_H

#include <QObject>
#include <QEvent>

class QnEventSignalizer: public QObject {
    Q_OBJECT;

public:
    QnEventSignalizer(QObject *parent = NULL): QObject(parent) {}

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


class QnSingleEventSignalizer: public QnEventSignalizer {
    Q_OBJECT;

public:
    QnSingleEventSignalizer(QObject *parent = NULL): QnEventSignalizer(parent), m_type(QEvent::None) {}

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


#endif // QN_EVENT_SIGNALIZER_H
