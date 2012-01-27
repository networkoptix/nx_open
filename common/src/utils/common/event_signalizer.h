#ifndef QN_EVENT_SIGNALIZER_H
#define QN_EVENT_SIGNALIZER_H

#include <QObject>
#include <QEvent>
#include "warnings.h"

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


class QnMultiEventSignalizer: public QnEventSignalizer {
    Q_OBJECT;

public:
    QnMultiEventSignalizer(QObject *parent = NULL): QnEventSignalizer(parent) {}

    void addEventType(QEvent::Type type) {
        if(type < 0) {
            qnWarning("Invalid event type '%1'.", static_cast<int>(type));
            return;
        }

        while(m_typeMask.size() <= type)
            m_typeMask.push_back(false);

        m_typeMask[type] = true;
    }

    void clearEventTypes() {
        m_typeMask.resize(0);
    }

protected:
    virtual bool isActivating(QEvent *event) const override {
        if(event->type() < 0 || event->type() >= m_typeMask.size())
            return false;

        return m_typeMask[event->type()];
    }

private:
    QVector<bool> m_typeMask;
};


#endif // QN_EVENT_SIGNALIZER_H
