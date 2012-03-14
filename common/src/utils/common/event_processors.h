#ifndef QN_EVENT_PROCESSORS_H
#define QN_EVENT_PROCESSORS_H

#include <QObject>
#include <QEvent>
#include "warnings.h"

template<class Base>
class QnEventProcessor: public Base {
public:
    QnEventProcessor(QObject *parent = NULL): Base(parent) {}

    virtual bool eventFilter(QObject *watched, QEvent *event) override {
        if(isActivating(watched, event))
            if(activate(watched, event))
                return true;

        return Base::eventFilter(watched, event);
    }

protected:
    virtual bool isActivating(QObject *watched, QEvent *event) const = 0;
    virtual bool activate(QObject *watched, QEvent *event) = 0;
};


template<class Base>
class QnSingleEventProcessor: public QnEventProcessor<Base> {
public:
    QnSingleEventProcessor(QObject *parent = NULL): QnEventProcessor<Base>(parent), m_type(QEvent::None) {}

    void setEventType(QEvent::Type type) {
        m_type = type;
    }

    QEvent::Type eventType() const {
        return m_type;
    }

protected:
    virtual bool isActivating(QObject *, QEvent *event) const override {
        return event->type() == m_type;
    }

private:
    QEvent::Type m_type;
};


template<class Base>
class QnMultiEventProcessor: public QnEventProcessor<Base> {
public:
    QnMultiEventProcessor(QObject *parent = NULL): QnEventProcessor<Base>(parent) {}

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
    virtual bool isActivating(QObject *, QEvent *event) const override {
        if(event->type() < 0 || event->type() >= m_typeMask.size())
            return false;

        return m_typeMask[event->type()];
    }

private:
    QVector<bool> m_typeMask;
};


class QnEventSignalizerBase: public QObject {
    Q_OBJECT;
public:
    QnEventSignalizerBase(QObject *parent = NULL): QObject(parent) {}

signals:
    void activated(QObject *object, QEvent *event);
};


template<class Base>
class QnEventSignalizer: public Base {
public:
    QnEventSignalizer(QObject *parent = NULL): Base(parent) {}

protected:
    virtual bool activate(QObject *watched, QEvent *event) override {
        emit Base::activated(watched, event);

        return false;
    }
};


typedef QnEventSignalizer<QnSingleEventProcessor<QnEventSignalizerBase> > QnSingleEventSignalizer;
typedef QnEventSignalizer<QnMultiEventProcessor<QnEventSignalizerBase> >  QnMultiEventSignalizer;

namespace Qn {
    enum EventProcessingPolicy {
        AcceptEvent,
        IgnoreEvent,
        KeepEvent
    };
}

template<class Base>
class QnEventEater: public Base {
public:
    QnEventEater(QObject *parent = NULL): Base(parent), m_policy(Qn::KeepEvent) {}

    QnEventEater(Qn::EventProcessingPolicy policy, QObject *parent = NULL): Base(parent), m_policy(policy) {}

protected:
    virtual bool activate(QObject *, QEvent *event) override {
        switch(m_policy) {
        case Qn::AcceptEvent:
            event->accept();
            break;
        case Qn::IgnoreEvent:
            event->ignore();
            break;
        default:
            break;
        }

        return true;
    }

private:
    Qn::EventProcessingPolicy m_policy;
};

typedef QnEventEater<QnSingleEventProcessor<QObject> > QnSingleEventEater;
typedef QnEventEater<QnMultiEventProcessor<QObject> > QnMultiEventEater;


#endif // QN_EVENT_PROCESSORS_H
