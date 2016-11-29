#pragma once

#include <utility>

#include <QtCore/QObject>
#include <QtCore/QEvent>
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

/* Container version of createEventSignalizer function: */
template<class Events = std::initializer_list<QEvent::Type>>
QnAbstractEventSignalizer* createEventSignalizer(
    const Events& events,
    QObject* parent = nullptr)
{
    switch (events.size())
    {
        case 0:
            return nullptr;

        case 1:
        {
            auto singleEventSignalizer = new QnSingleEventSignalizer(parent);
            singleEventSignalizer->setEventType(*events.begin());
            return singleEventSignalizer;
        }

        default:
        {
            auto multiEventSignalizer = new QnMultiEventSignalizer(parent);
            for (auto type : events)
                multiEventSignalizer->addEventType(type);

            return multiEventSignalizer;
        }
    }
}

/* Single event version of createEventSignalizer function: */
inline QnAbstractEventSignalizer* createEventSignalizer(
    QEvent::Type event,
    QObject* parent)
{
    auto singleEventSignalizer = new QnSingleEventSignalizer(parent);
    singleEventSignalizer->setEventType(event);
    return singleEventSignalizer;
}

/* Container version of installEventFilter function (SFINAE overload): */
template<
    class Watched = std::initializer_list<QObject*>,
    class = decltype(std::declval<Watched>().size()),  //< size(), begin() and end()
    class = decltype(std::declval<Watched>().begin()), //<     functions
    class = decltype(std::declval<Watched>().end())>   //<         must exist
bool installEventFilter(const Watched& watched, QObject* filter)
{
    if (watched.size() == 0 || !filter)
        return false;

    for (auto object: watched)
        object->installEventFilter(filter);

    return true;
}

/* Raw and smart pointer version of installEventFilter function (SFINAE overload): */
template<
    class Watched,
    /* Bool typecast availability check: */
    class = decltype(static_cast<bool>(std::declval<Watched>())),
    /* Dereferenceability and installEventFilter method existance check: */
    class = decltype(std::declval<Watched>()->installEventFilter(nullptr))>
bool installEventFilter(const Watched& watched, QObject* filter)
{
    if (!watched || !filter)
        return false;

    watched->installEventFilter(filter);
    return true;
}

/**
* Connects to object(s) event(s) and calls receiver's handler.
*   watched - raw or smart pointer to QObject descendant,
*      an initializer list or a container of pointers to QObject descendants
*   events - a single value of QEvent::Type, an initializer list
*      or a container of QEvent::Type items
*   receiver - raw or smart pointer to QObject descendant to handle signals
*   handler - member function or functor that will be called to handle signals
*   connectionType - Qt signal/slot connection type
*      Qt::UniqueConnection is currently not supported
*      Qt::QueuedConnection should be used with care: unguarded pointer to watched object
*          is passed to the handler function, so user must provide own guard measures
*/
template<
    class Watched = std::initializer_list<QObject*>,
    class Events = std::initializer_list<QEvent::Type>,
    class Receiver,
    class HandlerFunc>
QnAbstractEventSignalizer* installEventHandler(
    const Watched& watched,
    const Events& events,
    const Receiver& receiver,
    HandlerFunc handler,
    Qt::ConnectionType connectionType = Qt::AutoConnection)
{
    auto receiverObject = &*receiver; //< smart to raw pointer
    if (!receiver)
        return nullptr;

    QScopedPointer<QnAbstractEventSignalizer> scopedSignalizer(
        createEventSignalizer(events, receiverObject));

    if (!scopedSignalizer)
        return nullptr;

    auto signalizer = scopedSignalizer.data();

    if (!installEventFilter(watched, signalizer))
        return nullptr;

    if (!QObject::connect(signalizer, &QnAbstractEventSignalizer::activated,
        receiverObject, handler, connectionType))
    {
        return nullptr;
    }

    auto disconnectFunctor = [signalizer] { signalizer->disconnect(); };

    if (!QObject::connect(receiverObject, &QObject::destroyed, signalizer, disconnectFunctor))
        return nullptr;

    return scopedSignalizer.take(); //< pass scoped ownership to the caller
}

