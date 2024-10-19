// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

class QnLayoutsModel : public QSortFilterProxyModel
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

public:
    enum class ItemType
    {
        AllCameras,
        Layout
    };
    Q_ENUM(ItemType)

    QnLayoutsModel(QObject* parent = nullptr);

    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
