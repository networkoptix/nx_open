// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariant>
#include <QtCore/QModelIndex>

namespace nx::vms::client::desktop::testkit {

/**
 * Wrapper class for QObject for accessing some of its properties and methods
 * that are not directly exposed to scripting engine.
 */
class ObjectWrapper
{
    Q_GADGET

    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(QString objectName READ name)

public:
    ObjectWrapper(QObject* object = nullptr): m_object(object) {}

    Q_INVOKABLE QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    Q_INVOKABLE QObject* model();
    Q_INVOKABLE int rowCount(const QModelIndex& parent = QModelIndex()) const;
    Q_INVOKABLE int columnCount(const QModelIndex& parent = QModelIndex()) const;
    Q_INVOKABLE QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;

    Q_INVOKABLE void setText(QString text);
    Q_INVOKABLE int findText(QString text) const;

    Q_INVOKABLE void activate();
    Q_INVOKABLE void activateWindow();

    Q_INVOKABLE QVariantMap metaInfo(QString name) const;

    QString name() const { return m_object->objectName(); }
    QString type() const { return m_object->metaObject()->className(); }

private:
    QPointer<QObject> m_object;
};

} // namespace nx::vms::client::desktop::testkit
