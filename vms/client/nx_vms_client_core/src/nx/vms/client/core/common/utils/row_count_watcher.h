// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API RowCountWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QModelIndex parentIndex READ parentIndex WRITE setParentIndex
        NOTIFY parentIndexChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(bool recursiveTracking READ recursiveTracking WRITE setRecursiveTracking
        NOTIFY recursiveTrackingChanged)
    Q_PROPERTY(int recursiveRowCount READ recursiveRowCount NOTIFY recursiveRowCountChanged)

    using base_type = QObject;

public:
    explicit RowCountWatcher(QObject* parent = nullptr);
    virtual ~RowCountWatcher() override;

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* value);

    QModelIndex parentIndex() const;
    void setParentIndex(const QModelIndex& value);

    // Whether `recursiveRowCount` is maintained. Default `false`.
    bool recursiveTracking() const;
    void setRecursiveTracking(bool value);

    int rowCount() const;
    int recursiveRowCount() const;

    static void registerQmlType();

signals:
    void modelChanged();
    void parentIndexChanged();
    void recursiveTrackingChanged();
    void rowCountChanged();
    void recursiveRowCountChanged();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
