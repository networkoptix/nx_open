// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCollator>
#include <QtCore/QSortFilterProxyModel>

namespace nx::vms::client::desktop::integrations {

class ImportFromDeviceFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT

public:
    Q_PROPERTY(QString filterWildcard READ filterWildcard
        WRITE setFilterWildcard NOTIFY filterWildcardChanged)

public:
    explicit ImportFromDeviceFilterModel(QObject* parent = nullptr);

    QString filterWildcard() const;
    void setFilterWildcard(const QString& value);

signals:
    void filterWildcardChanged();

private:
    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_filterWildcard;
    QCollator m_collator;
};

} // namespace nx::vms::client::desktop::integrations
