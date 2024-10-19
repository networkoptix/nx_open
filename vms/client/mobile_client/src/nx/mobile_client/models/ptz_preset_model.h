// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QUuid>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace mobile {

class PtzPresetModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(nx::Uuid resourceId READ resourceId
        WRITE setResourceId NOTIFY resourceIdChanged)

public:
    PtzPresetModel(QObject* parent = nullptr);
    virtual ~PtzPresetModel();

    nx::Uuid resourceId() const;
    void setResourceId(const nx::Uuid& value);

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void resourceIdChanged();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace mobile
} // namespace client
} // namespace nx
