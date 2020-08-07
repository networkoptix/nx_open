#pragma once

#include <QtCore/QObject>
#include <QtGui/QCursor>

// Narrowest standard include to use QML_DECLARE_TYPEINFO.
#include <QtQml/QQmlComponent>

namespace nx::vms::client::desktop {

class CustomCursorAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QCursor cursor READ cursor WRITE setCursor RESET unsetCursor)

public:
    explicit CustomCursorAttached(QObject* parent = nullptr);

    QCursor cursor() const;
    void setCursor(QCursor value);
    void unsetCursor();
};

class CustomCursor: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject; //< Forward constructors.

    static CustomCursorAttached* qmlAttachedProperties(QObject* parent);
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop

QML_DECLARE_TYPEINFO(nx::vms::client::desktop::CustomCursor, QML_HAS_ATTACHED_PROPERTIES)
