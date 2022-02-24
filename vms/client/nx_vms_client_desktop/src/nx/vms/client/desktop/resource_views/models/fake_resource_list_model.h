// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtGui/QIcon>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct FakeResourceDescription
{
    QString id;
    QIcon icon;
    QString name;
    QString address;
};

using FakeResourceList = QList<FakeResourceDescription>;

class FakeResourceListModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    enum Columns
    {
        iconColumn,
        nameColumn,
        addressColumn,

        count
    };

    explicit FakeResourceListModel(
        const FakeResourceList& fakeResources,
        QObject* parent = nullptr);

    virtual ~FakeResourceListModel() override;

    FakeResourceList resources() const;

public:
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;

    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

private:
    FakeResourceList m_resources;
};

} // namespace nx::vms::client::desktop
