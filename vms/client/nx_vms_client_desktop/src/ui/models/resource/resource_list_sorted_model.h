// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QCollator>

#include <core/resource/resource_fwd.h>

class QnResourceListSortedModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;
public:
    explicit QnResourceListSortedModel(QObject* parent = nullptr);
    virtual ~QnResourceListSortedModel();

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QCollator m_collator;
};
