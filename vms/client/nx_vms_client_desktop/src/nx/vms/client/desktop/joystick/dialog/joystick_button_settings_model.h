// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

#include "../settings/device.h"

namespace nx::vms::client::desktop {

class FilteredResourceProxyModel;

namespace joystick {

struct JoystickDescriptor;

class JoystickButtonSettingsModel: public QAbstractTableModel, public WindowContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString modifierButtonName READ modifierButtonName NOTIFY modifierButtonNameChanged)
    Q_PROPERTY(bool zoomIsEnabled READ zoomIsEnabled NOTIFY zoomIsEnabledChanged)
    Q_PROPERTY(qreal panAndTiltSensitivity
        READ panAndTiltSensitivity
        WRITE setPanAndTiltSensitivity
        NOTIFY panAndTiltSensitivityChanged)
    Q_PROPERTY(qreal zoomSensitivity
        READ zoomSensitivity
        WRITE setZoomSensitivity
        NOTIFY zoomSensitivityChanged)

    using base_type = QAbstractTableModel;

public:
    enum Roles
    {
        ActionParameterTypeRole = Qt::UserRole + 1,
        ActionIdRole,
        HotkeyIndexRole,
        LayoutLogicalIdRole,
        LayoutUuidRole,
        IsModifierRole,
        IsEnabledRole,
        ButtonPressedRole
    };
    Q_ENUM(Roles)

    enum ActionParameterTypes
    {
        NoParameter,
        HotkeyParameter,
        LayoutParameter
    };
    Q_ENUM(ActionParameterTypes)

    enum ColumnType
    {
        FirstColumn,
        ButtonNameColumn = FirstColumn,
        BaseActionColumn,
        BaseActionParameterColumn,
        ModifiedActionColumn,
        ModifiedActionParameterColumn,
        LastColumn = ModifiedActionParameterColumn
    };
    Q_ENUM(ColumnType)

public:
    JoystickButtonSettingsModel(
        WindowContext* windowContext,
        FilteredResourceProxyModel* resourceModel,
        QObject* parent);
    virtual ~JoystickButtonSettingsModel() override;

    void init(const JoystickDescriptor& description, const Device* device);

    JoystickDescriptor getDescriptionState() const;

    QString modifierButtonName() const;

    bool zoomIsEnabled() const;

    double panAndTiltSensitivity() const;
    void setPanAndTiltSensitivity(double value);

    double zoomSensitivity() const;
    void setZoomSensitivity(double value);

    bool openLayoutActionIsSet() const;

public:
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

signals:
    void modifierButtonNameChanged();
    void zoomIsEnabledChanged();
    void panAndTiltSensitivityChanged(bool initialization = false);
    void zoomSensitivityChanged(bool initialization = false);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
} // namespace nx::vms::client::desktop
