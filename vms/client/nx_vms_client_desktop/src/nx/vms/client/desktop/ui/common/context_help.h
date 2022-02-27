// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

// Narrowest standard include to use QML_DECLARE_TYPEINFO.
#include <QtQml/QQmlComponent>

namespace nx::vms::client::desktop {

class ContextHelpAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(int topicId READ topicId WRITE setTopicId NOTIFY topicIdChanged)

public:
    explicit ContextHelpAttached(QObject* parent = nullptr);

    int topicId() const;
    void setTopicId(int value);

signals:
    void topicIdChanged();
};

class ContextHelp: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject; //< Forward constructors.

    static ContextHelpAttached* qmlAttachedProperties(QObject* parent);
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop

QML_DECLARE_TYPEINFO(nx::vms::client::desktop::ContextHelp, QML_HAS_ATTACHED_PROPERTIES)
