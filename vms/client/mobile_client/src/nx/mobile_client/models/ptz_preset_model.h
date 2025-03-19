// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QUuid>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx {
namespace client {
namespace mobile {

class PtzPresetModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(QnResource* resource READ rawResource
        WRITE setRawResource NOTIFY resourceChanged)

public:
    PtzPresetModel(QObject* parent = nullptr);
    virtual ~PtzPresetModel();

    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void resourceChanged();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace mobile
} // namespace client
} // namespace nx
