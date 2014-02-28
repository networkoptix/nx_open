#ifndef QN_EVENT_PROCESSORS_H
#define QN_EVENT_PROCESSORS_H

#include <QtCore/QObject>
#include <QtCore/QEvent>
#include "warnings.h"

// TODO: #Elric add syntax: connect(QObject *, QEvent::Type, QObject *, const char * /* slot (QObject *, QEvent *) */)

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

template<class Base>
class QnEveryEventProcessor: public QnEventProcessor<Base> {
public:
    QnEveryEventProcessor(QObject *parent = NULL): QnEventProcessor<Base>(parent) {}

protected:
    virtual bool isActivating(QObject *, QEvent *) const override {
        return true;
    }
};


class QnAbstractEventSignalizer: public QObject {
    Q_OBJECT;
public:
    QnAbstractEventSignalizer(QObject *parent = NULL): QObject(parent) {}

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


typedef QnEventSignalizer<QnSingleEventProcessor<QnAbstractEventSignalizer> > QnSingleEventSignalizer;
typedef QnEventSignalizer<QnMultiEventProcessor<QnAbstractEventSignalizer> >  QnMultiEventSignalizer;
typedef QnEventSignalizer<QnEveryEventProcessor<QnAbstractEventSignalizer> >  QnEveryEventSignalizer;


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
typedef QnEventEater<QnEveryEventProcessor<QObject> > QnEveryEventEater;

#define QN_EVENT_SIGNAL_EMITTER_SIGNAL_COUNT 60

/**
 * This class is used to emit signals from <tt>QnEventProcessor</tt>.
 *
 * Each emitter has 60 connection slots. This is due to the fact that <tt>QObject</tt> 
 * can quickly determine if the signal is connected only for the first 64 signals 
 * (As of Qt 4.7, at least; see <tt>QObjectPrivate::connectedSignals</tt>).
 * Each <tt>QObject</tt> defines a <tt>destroyed</tt> signal, so we have 63
 * signals left. 
 */
class QnEventSignalEmitter: public QObject {
    Q_OBJECT;
public:
    enum {
        SignalCount = QN_EVENT_SIGNAL_EMITTER_SIGNAL_COUNT
    };

    QnEventSignalEmitter(QObject *parent = NULL): QObject(parent) {}

    void activate(int signalIndex, QObject *watched, QEvent *event);

    static const char *signature(int signalIndex);

signals:
    /* This code is processed by MOC, so we cannot use boost preprocessor to 
     * autogenerate it. */
    void activate0(QObject *watched, QEvent *event);
    void activate1(QObject *watched, QEvent *event);
    void activate2(QObject *watched, QEvent *event);
    void activate3(QObject *watched, QEvent *event);
    void activate4(QObject *watched, QEvent *event);
    void activate5(QObject *watched, QEvent *event);
    void activate6(QObject *watched, QEvent *event);
    void activate7(QObject *watched, QEvent *event);
    void activate8(QObject *watched, QEvent *event);
    void activate9(QObject *watched, QEvent *event);
    void activate10(QObject *watched, QEvent *event);
    void activate11(QObject *watched, QEvent *event);
    void activate12(QObject *watched, QEvent *event);
    void activate13(QObject *watched, QEvent *event);
    void activate14(QObject *watched, QEvent *event);
    void activate15(QObject *watched, QEvent *event);
    void activate16(QObject *watched, QEvent *event);
    void activate17(QObject *watched, QEvent *event);
    void activate18(QObject *watched, QEvent *event);
    void activate19(QObject *watched, QEvent *event);
    void activate20(QObject *watched, QEvent *event);
    void activate21(QObject *watched, QEvent *event);
    void activate22(QObject *watched, QEvent *event);
    void activate23(QObject *watched, QEvent *event);
    void activate24(QObject *watched, QEvent *event);
    void activate25(QObject *watched, QEvent *event);
    void activate26(QObject *watched, QEvent *event);
    void activate27(QObject *watched, QEvent *event);
    void activate28(QObject *watched, QEvent *event);
    void activate29(QObject *watched, QEvent *event);
    void activate30(QObject *watched, QEvent *event);
    void activate31(QObject *watched, QEvent *event);
    void activate32(QObject *watched, QEvent *event);
    void activate33(QObject *watched, QEvent *event);
    void activate34(QObject *watched, QEvent *event);
    void activate35(QObject *watched, QEvent *event);
    void activate36(QObject *watched, QEvent *event);
    void activate37(QObject *watched, QEvent *event);
    void activate38(QObject *watched, QEvent *event);
    void activate39(QObject *watched, QEvent *event);
    void activate40(QObject *watched, QEvent *event);
    void activate41(QObject *watched, QEvent *event);
    void activate42(QObject *watched, QEvent *event);
    void activate43(QObject *watched, QEvent *event);
    void activate44(QObject *watched, QEvent *event);
    void activate45(QObject *watched, QEvent *event);
    void activate46(QObject *watched, QEvent *event);
    void activate47(QObject *watched, QEvent *event);
    void activate48(QObject *watched, QEvent *event);
    void activate49(QObject *watched, QEvent *event);
    void activate50(QObject *watched, QEvent *event);
    void activate51(QObject *watched, QEvent *event);
    void activate52(QObject *watched, QEvent *event);
    void activate53(QObject *watched, QEvent *event);
    void activate54(QObject *watched, QEvent *event);
    void activate55(QObject *watched, QEvent *event);
    void activate56(QObject *watched, QEvent *event);
    void activate57(QObject *watched, QEvent *event);
    void activate58(QObject *watched, QEvent *event);
    void activate59(QObject *watched, QEvent *event);
};

class QnEventSignalEmitterPool: public QnEventSignalEmitter {
    Q_OBJECT;

    typedef QnEventSignalEmitter base_type;

public:
    struct Signal {
        Signal(): sender(NULL), signalIndex(-1) {}
        Signal(QnEventSignalEmitter *sender, int signalIndex): sender(sender), signalIndex(signalIndex) {}

        QnEventSignalEmitter *sender;
        int signalIndex;
    };

    QnEventSignalEmitterPool(QObject *parent = NULL);

    Signal allocate();
    void release(const Signal &signal);

private:
    void grow(QnEventSignalEmitter *emitter);

private:
    QVector<Signal> m_freeList;
};


#endif // QN_EVENT_PROCESSORS_H
