// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

/**
 * An utility filtering model that limits the number of its rows.
 */
class LimitedModel: public QSortFilterProxyModel
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

    /** Limit for the number of items shown by the model. */
    Q_PROPERTY(int limit
        READ limit
        WRITE setLimit
        NOTIFY limitChanged)

    Q_PROPERTY(int remaining READ remaining NOTIFY remainingChanged)

public:
    LimitedModel();

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    void setLimit(int limit);
    int limit() const { return m_limit; }

    int remaining() const { return m_remaining; }

    static void registerQmlType();

signals:
    void limitChanged();
    void remainingChanged();
    void sourceCountChanged();

private slots:
    void updateRemaining();
    void sourceRowsChanged();

private:
    int m_limit = 0;
    int m_remaining = 0;
    nx::utils::ScopedConnections m_modelConnections;
};

} // namespace nx::vms::client::desktop
