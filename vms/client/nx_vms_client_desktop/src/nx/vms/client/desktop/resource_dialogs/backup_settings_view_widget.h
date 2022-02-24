// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/client/desktop/resource_dialogs/detailed_resource_tree_widget.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace nx::vms::client::desktop {

class BackupSettingsDecoratorModel;
class BackupSettingsItemDelegate;

class BackupSettingsViewWidget:
    public DetailedResourceTreeWidget,
    public QnAbstractPreferencesInterface
{
    Q_OBJECT
    using base_type = DetailedResourceTreeWidget;

public:
    BackupSettingsViewWidget(QWidget* parent);
    virtual ~BackupSettingsViewWidget() override;

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;

    nx::vms::api::BackupSettings globalBackupSettings() const;

signals:
    void hasChangesChanged();
    void globalBackupSettingsChanged();

protected:
    virtual QAbstractItemModel* model() const override;
    virtual void setupHeader() override;

private:
    void switchItemClicked(const QModelIndex& index);
    void dropdownItemClicked(const QModelIndex& index);

private:
    const std::unique_ptr<BackupSettingsDecoratorModel> m_backupSettingsDecoratorModel;
    const std::unique_ptr<BackupSettingsItemDelegate> m_viewItemDelegate;
};

} // namespace nx::vms::client::desktop
