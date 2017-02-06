#ifndef QN_CUSTOMIZED_H
#define QN_CUSTOMIZED_H

#ifndef Q_MOC_RUN
#include <boost/type_traits/is_base_of.hpp>
#endif

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QCoreApplication>

#include <utils/common/forward.h>

#include "customizer.h"


namespace QnEvent {
    static const QEvent::Type Customize = static_cast<QEvent::Type>(QEvent::User + 0x8A5E);
}


template<class Base, bool baseIsCustomized>
class Customized;


/**
 * Base class for objects that are to be passed through customization subsystem
 * after construction. Note that this can be done automatically for <tt>QApplication</tt>
 * and <tt>QWidget</tt>s from <tt>QStyle</tt> implementation.
 *
 * Not really useful for anything, but can be <tt>dynamic_cast</tt>ed to.
 */
class CustomizedBase {
private:
    CustomizedBase() {}
    virtual ~CustomizedBase() {}

    template<class Base, bool baseIsCustomized>
    friend class ::Customized; /* So that only this class can access our methods. */
};


/**
 * Convenience base class for objects that need to be processed through
 * customization subsystem.
 */
template<class Base, bool baseIsCustomized = boost::is_base_of<CustomizedBase, Base>::value>
class Customized: public Base, public CustomizedBase {
public:
    QN_FORWARD_CONSTRUCTOR(Customized, Base, { QCoreApplication::postEvent(this, new QEvent(QnEvent::Customize), Qt::HighEventPriority); });

protected:
    virtual bool event(QEvent *event) override {
        if(event->type() == QnEvent::Customize) {
            qnCustomizer->customize(this); // TODO: #Elric do we need a null check here?
            return true;
        } else {
            return Base::event(event);
        }
    }
};


template<class Base>
class Customized<Base, true>: public Base {
public:
    QN_FORWARD_CONSTRUCTOR(Customized, Base, {});
};


#endif // QN_CUSTOMIZED_H
