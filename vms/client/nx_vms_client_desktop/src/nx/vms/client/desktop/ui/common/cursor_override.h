#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

// Narrowest standard include to use QML_DECLARE_TYPEINFO.
#include <QtQml/QQmlComponent>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API CursorOverrideAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(Qt::CursorShape shape READ shape WRITE setShape NOTIFY shapeChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit CursorOverrideAttached(QObject* parent = nullptr);
    virtual ~CursorOverrideAttached() override;

    Qt::CursorShape shape() const;
    void setShape(Qt::CursorShape value);

    bool active() const;
    void setActive(bool value);

    Q_INVOKABLE bool effectivelyActive() const;

signals:
    void shapeChanged(QPrivateSignal);
    void activeChanged(QPrivateSignal);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class CursorOverride: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject; //< Forward constructors.

    static CursorOverrideAttached* qmlAttachedProperties(QObject* parent);
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop

QML_DECLARE_TYPEINFO(nx::vms::client::desktop::CursorOverride, QML_HAS_ATTACHED_PROPERTIES)
