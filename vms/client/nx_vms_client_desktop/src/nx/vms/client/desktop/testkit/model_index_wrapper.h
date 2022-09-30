// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariant>
#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop::testkit {

/**
 * Wrapper class for QModelIndex bound with its visual parent.
 * It also exposes more properties of QModelIndex to scripting.
 */
class ModelIndexWrapper
{
    Q_GADGET
    Q_PROPERTY(int row READ row)
    Q_PROPERTY(int column READ column)
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(QString checkState READ checkState)
    Q_PROPERTY(QString toolTip READ toolTip)
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(QString objectName READ name)
    Q_PROPERTY(bool selected READ isSelected)
    Q_PROPERTY(bool isSelected READ isSelected)

public:
    ModelIndexWrapper(QModelIndex index = QModelIndex(), QWidget* container = nullptr):
        m_index(index),
        m_container(container)
    {}

    int row() const { return m_index.row(); }
    int column() const { return m_index.column(); }
    QString text() const { return m_index.data().toString(); }
    QString checkState() const { return checkStateName(m_index.data(Qt::CheckStateRole)); }
    QString toolTip() const { return m_index.data(Qt::ToolTipRole).toString(); }
    QString name() const { return QString(); }
    QString type() const { return "QModelIndex"; }

    Q_INVOKABLE QVariant data(int role = Qt::DisplayRole) const;

    Q_INVOKABLE bool isValid() const { return m_index.isValid(); }

    QModelIndex index() const { return m_index; }
    QWidget* container() const { return m_container; }

    bool operator<(const ModelIndexWrapper& other) const { return m_index < other.m_index; }
    bool operator==(const ModelIndexWrapper& other) const = default;
    bool isSelected() const;

    Q_INVOKABLE QVariantMap metaInfo(QString name) const;

    // Register converters and comparators, can be called multiple times.
    static void registerMetaType();

    // Get string representation of CheckStateRole.
    static QString checkStateName(QVariant state);

private:
    QModelIndex m_index;
    QPointer<QWidget> m_container;
};

} // namespace nx::vms::client::desktop::testkit
