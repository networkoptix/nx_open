// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/camera/buttons/abstract_camera_button_controller.h>

namespace nx::vms::client::core { class AbstractCameraButtonController; }

namespace nx::vms::client::mobile {

/** Represents model for camera buttons. */
class CameraButtonsModel: public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

    Q_PROPERTY(nx::vms::client::core::AbstractCameraButtonController* controller
        READ controller
        WRITE setController
        NOTIFY controllerChanged)

public:
    static void registerQmlType();

    CameraButtonsModel(QObject* parent = nullptr);

    virtual ~CameraButtonsModel() override;

    core::AbstractCameraButtonController* controller() const;
    void setController(core::AbstractCameraButtonController* value);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int rowById(const nx::Uuid& buttonId) const;

signals:
    void controllerChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
