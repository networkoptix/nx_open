// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::joystick {

class JoystickButtonActionChoiceModel: public QAbstractListModel, public WindowContextAware
{
    Q_OBJECT

    using base_type = QAbstractListModel;

    struct ActionInfo
    {
        bool isSeparator = true;
        menu::IDType id = menu::NoAction;
        QString name;

        ActionInfo()
        {}

        ActionInfo(menu::IDType actionId, const QString& name):
            isSeparator(false),
            id(actionId),
            name(name)
        {}
    };

    static ActionInfo separator();
    static QVector<ActionInfo> itemsBeforeOpenLayoutItem();
    static QVector<ActionInfo> openLayoutItems();
    static QVector<ActionInfo> itemsAfterOpenLayoutItem();
    static QVector<ActionInfo> modifierItems();

public:
    enum Roles
    {
        ActionNameRole = Qt::UserRole + 1,
        ActionIdRole,
        ButtonsRole,
        IsSeparatorRole,
        IsEnabledRole
    };
    Q_ENUM(Roles)

public:
    JoystickButtonActionChoiceModel(
        WindowContext* windowContext, bool withModifier, QObject* parent = nullptr);
    virtual ~JoystickButtonActionChoiceModel() override;

    void initButtons(
        const QHash<QString, menu::IDType>& baseActions,
        const QHash<QString, menu::IDType>& modifiedActions);

    void setOpenLayoutChoiceEnabled(bool enabled);
    void setOpenLayoutChoiceVisible(bool visible);

    Q_INVOKABLE int getRow(const QVariant& actionId) const;
    Q_INVOKABLE void setActionButton(
        menu::IDType actionId,
        const QString& buttonName,
        bool withModifier = false);

public:
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex& index, int role = ActionIdRole) const override;

    QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
