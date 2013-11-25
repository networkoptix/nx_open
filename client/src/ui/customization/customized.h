#ifndef QN_CUSTOMIZABLE_H
#define QN_CUSTOMIZABLE_H

#include <boost/type_traits/is_base_of.hpp>

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QCoreApplication>

#include <utils/common/forward.h>

#include "customizer.h"


// TODO: #Elric check if the name is correct
namespace QnEvent {
    static const QEvent::Type Customize = static_cast<QEvent::Type>(QEvent::User + 0x8A5E);
}


template<class Base, bool baseIsCustomized>
class Customized;


// TODO: #Elric docz
class CustomizedBase {
private:
    CustomizedBase() {}
    virtual ~CustomizedBase() {}

    template<class Base, bool baseIsCustomized>
    friend class ::Customized; /* So that only this class can access our methods. */
};


// TODO: #Elric docz
template<class Base, bool baseIsCustomized = boost::is_base_of<CustomizedBase, Base>::value>
class Customized: public Base, public CustomizedBase {
public:
    QN_FORWARD_CONSTRUCTOR(Customized, Base, { QCoreApplication::postEvent(this, new QEvent(QnEvent::Customize)); });

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


#endif // QN_CUSTOMIZABLE_H
