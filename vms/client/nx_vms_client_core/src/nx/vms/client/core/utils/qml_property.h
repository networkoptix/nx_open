// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <QtQml/QQmlProperty>

class QQuickView;

namespace nx::vms::client::core {

namespace detail {

class NX_VMS_CLIENT_CORE_API QmlPropertyConnection: public QObject
{
    Q_OBJECT

public:
    virtual void setTarget(QObject* object, const QString& propertyName) = 0;

signals:
    void valueChanged();
};

} // namespace detail

class NX_VMS_CLIENT_CORE_API QmlPropertyBase
{
    class SharedState;

public:
    /**
     * An interface class to allow creating QmlProperty instances which use a dynamic object
     * without need to derive from a template class QmlProperty.
     *
     * A class which wants to use a QmlProperty with a dynamic object should use this holder in the
     * corresponding constructor of QmlProperty.
     */
    class NX_VMS_CLIENT_CORE_API ObjectHolder
    {
    public:
        QObject* object() const;
        void setObject(QObject* object);

    private:
        void notifyProperties();

        /**
         * Registers property to be notifyed when the object is changed.
         * Called from the correspodinf constructor of BaseQmlProperty.
         */
        void addProperty(QmlPropertyBase* property);

        friend class QmlPropertyBase;

        QObject* m_object = nullptr;
        QList<std::weak_ptr<QmlPropertyBase::SharedState>> m_properties;
    };

public:
    /** Constructs an empty invalid property. */
    QmlPropertyBase();

    /** Constructs a property with a given fixed object. */
    QmlPropertyBase(QObject* object, const QString& propertyName);

    /**
     * Constructs a property which uses a dynamic QQuickView::rootObject() as the property object.
     *
     * QQuickView::rootObject() can be changed by loading another QML component. This property will
     * re-configure itself to use new object on every rootObject change.
     */
    QmlPropertyBase(QQuickView* quickView, const QString& propertyName);

    /**
     * Constructs a property which uses a dynamic ObjectHolder::object() as the property object.
     *
     * The property constructed this way will act the same way as a property configured with
     * QQuickView, but will use ObjectHolder interface to get the root object. This may be
     * used in user classes to provide a dynamic object for the property.
     *
     * See ObjectHolder.
     */
    QmlPropertyBase(ObjectHolder* holder, const QString& propertyName);

    /**
     * Constructs a property which uses the value of another QML property as the property object.
     *
     * The property constructed this way connects to the property notify signal and changes the
     * object each time the property changes. If the property contains anything but QObject* or
     * null then an assertion is raised.
     */
    QmlPropertyBase(const QmlPropertyBase* objectProperty, const QString& propertyName);

    virtual ~QmlPropertyBase() = default;

    bool isValid() const;

    bool hasNotifySignal() const;

    QMetaObject::Connection connectNotifySignal(QObject* receiver, const char* slot) const;

    template<typename Receiver, typename Func>
    QMetaObject::Connection connectNotifySignal(Receiver receiver, Func method) const
    {
        return QObject::connect(connection(), &detail::QmlPropertyConnection::valueChanged,
            receiver, method);
    }

    template<typename Func>
    QMetaObject::Connection connectNotifySignal(QObject* receiver, Func method) const
    {
        return QObject::connect(connection(), &detail::QmlPropertyConnection::valueChanged,
            receiver, method);
    }

    template<typename Func>
    QMetaObject::Connection connectNotifySignal(Func function) const
    {
        return QObject::connect(connection(), &detail::QmlPropertyConnection::valueChanged, function);
    }

protected:
    QQmlProperty property() const;
    detail::QmlPropertyConnection* connection() const;

private:
    std::shared_ptr<SharedState> m_state;
};

/**
 * A helper which makes access to QML properties easier.
 *
 * Advantages over QQmlProperty:
 *   - Uses overloads to make value reading and writing shorter.
 *   - Declares a typed value.
 *   - Can connect the notify signal to a C++ functor.
 *
 * A simple usage example is a boolean property. Imagine you need to proxy QML bool property `foo`.
 * ```
 * // Define an instance of QmlProperty:
 * const QmlProperty<bool> foo(qmlObject, "foo");
 *
 * // Read the value by implicitly or explicitly casting to bool:
 * const bool value = foo;
 *
 * // Write a new value by simply assigning a bool value to `foo`:
 * foo = true;
 *
 * // Subscribe to value changes:
 * foo.connectNotifySignal([foo]() { qDebug() << foo(); });
 * ```
 *
 * Another use-case is exposing QML properties to C++ in a wrapper class. For this purpose
 * QmlProperty can track rootObject of QQuickView.
 * ```
 * class CredentialsDialog: public QQuickView
 * {
 * public:
 *     ...
 *     const QmlProperty<QString> login(this, "login");
 *     const QmlProperty<QString> password(this, "password");
 *     const QmlProperty<bool> savePassword(this, "savePassword");
 *     ...
 * };
 *
 * void getCredentials()
 * {
 *     CredentialsDialog dialog;
 *
 *     // Open the dialog and wait for input completion.
 *
 *     const QString login = dialog.login;
 *     const QString password = dialog.password;
 *     const bool savePassword = dialog.savePassword;
 * }
 * ```
 */
template<typename T>
class QmlProperty: public QmlPropertyBase
{
public:
    using QmlPropertyBase::QmlPropertyBase;

    T value() const;
    bool setValue(const T& value) const;

    T operator()() const { return value(); }
    operator T() const { return value(); }

    const T& operator=(const T& value) const
    {
        setValue(value);
        return value;
    }
};

template<typename T>
T QmlProperty<T>::value() const
{
    return property().read().template value<T>();
}

template<typename T>
bool QmlProperty<T>::setValue(const T& value) const
{
    return property().write(QVariant::fromValue(value));
}

template<>
NX_VMS_CLIENT_CORE_API QVariant QmlProperty<QVariant>::value() const;

template<>
NX_VMS_CLIENT_CORE_API bool QmlProperty<QVariant>::setValue(const QVariant& value) const;

} // namespace nx::vms::client::core
