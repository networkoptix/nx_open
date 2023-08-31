// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_picker_widget.h"

#include "ui_backup_settings_picker_widget.h"

#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtGui/QKeySequence>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/backup_settings_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/models/backup_settings_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_store.h>
#include <ui/common/palette.h>
#include <utils/math/color_transformations.h>

namespace {

static constexpr auto kBackgroundHighlightPercentage = 7.5;

} // namespace

namespace nx::vms::client::desktop {

using namespace backup_settings_view;
using namespace nx::vms::api;

BackupSettingsPickerWidget::BackupSettingsPickerWidget(
    ServerSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::BackupSettingsPickerWidget())
{
    ui->setupUi(this);

    const auto keySequenceHintString =
        tr("Use %1 or %2 to select multiple devices for batch edit. Use %3 to select all devices.");

    ShortcutHintBuilder(ui->shortcutHintWidget)
        .withHintLine(keySequenceHintString)
        .withKeySequence(QKeySequence(Qt::Key_Control))
        .withKeySequence(QKeySequence(Qt::Key_Shift))
        .withKeySequence(QKeySequence(Qt::Key_Control, Qt::Key_A));

    const auto backgroundHighlightAlpha = std::rint(kBackgroundHighlightPercentage / 100.0 * 0xFF);
    const auto backgroundColor = alphaBlend(
        core::colorTheme()->color("dark7"),
        core::colorTheme()->color("brand_core", backgroundHighlightAlpha));

    setPaletteColor(this, QPalette::Window, backgroundColor);
    setAutoFillBackground(true);

    setPaletteColor(ui->horizontalLine, QPalette::Shadow, core::colorTheme()->color("dark6"));

    QFont selectionDescriptionLabelFont = ui->selectionDescriptionLabel->font();
    selectionDescriptionLabelFont.setWeight(QFont::DemiBold);
    ui->selectionDescriptionLabel->setFont(selectionDescriptionLabelFont);

    connect(ui->enableSwitch, &QCheckBox::stateChanged, this,
        [this](int state)
        {
            if (state == Qt::PartiallyChecked)
                return;

            ui->enableSwitch->setTristate(false);
            emit backupEnabledChanged(state == Qt::Checked);
        });

    ui->shortcutHintLayout->setSpacing(fontMetrics().horizontalAdvance(QChar::Space));

    setupContentTypesDropdown(store->state().backupStoragesStatus.usesCloudBackupStorage);
    setupQualityDropdown();

    connect(store, &ServerSettingsDialogStore::stateChanged,
        this, &BackupSettingsPickerWidget::loadState);
}

BackupSettingsPickerWidget::~BackupSettingsPickerWidget()
{
}

QString BackupSettingsPickerWidget::backupContentTypesPlaceholderText()
{
    return QString("<%1>").arg(tr("What to backup"));
}

QString BackupSettingsPickerWidget::backupQualityPlaceholderText()
{
    return QString("<%1>").arg(tr("Quality"));
}

void BackupSettingsPickerWidget::setupFromSelection(const QModelIndexList& indexes)
{
    QModelIndexList camerasIndexes;
    std::copy_if(std::cbegin(indexes), std::cend(indexes), std::back_inserter(camerasIndexes),
        [](const QModelIndex& index)
        {
            return index.column() == ResourceColumn && isBackupSupported(index);
        });

    if (camerasIndexes.size() > 1)
        ui->stackedWidget->setCurrentWidget(ui->pickerPage);
    else
        ui->stackedWidget->setCurrentWidget(ui->hintPage);

    QSet<int> selectionBackupTypes;
    QSet<int> selectionBackupQualities;
    QSet<bool> selectionBackupEnabledStates;

    for (const QModelIndex& cameraIndex: std::as_const(camerasIndexes))
    {
        const auto backupTypesData = cameraIndex.data(BackupContentTypesRole);
        const auto backupQualitiesData = cameraIndex.data(BackupQualityRole);
        const auto backupEnabledData = cameraIndex.data(BackupEnabledRole);

        if (!backupTypesData.isNull())
            selectionBackupTypes.insert(backupTypesData.toInt());

        if (!backupQualitiesData.isNull())
            selectionBackupQualities.insert(backupQualitiesData.toInt());

        if (!backupEnabledData.isNull())
            selectionBackupEnabledStates.insert(backupEnabledData.toBool());
    }

    ui->contentsTypesDropdown->setText(selectionBackupTypes.size() == 1
        ? backupContentTypesToString(static_cast<BackupContentTypes>(*selectionBackupTypes.cbegin()))
        : backupContentTypesPlaceholderText());

    ui->qualityDropdown->setText(selectionBackupQualities.size() == 1
        ? backupQualityToString(static_cast<CameraBackupQuality>(*selectionBackupQualities.cbegin()))
        : backupQualityPlaceholderText());

    if (selectionBackupEnabledStates.size() == 1)
    {
        QSignalBlocker signalBlocker(ui->enableSwitch);
        ui->enableSwitch->setCheckState(
            selectionBackupEnabledStates.contains(true) ? Qt::Checked : Qt::Unchecked);
        ui->enableSwitch->setTristate(false);
    }
    else
    {
        QSignalBlocker signalBlocker(ui->enableSwitch);
        ui->enableSwitch->setCheckState(Qt::PartiallyChecked);
    }

    const auto selectionHasDualStreamingDevices =
        std::any_of(camerasIndexes.cbegin(), camerasIndexes.cend(),
            [](const QModelIndex& cameraIndex)
            {
                const auto camera = cameraIndex.data(Qn::ResourceRole).value<QnResourcePtr>()
                    .dynamicCast<QnVirtualCameraResource>();
                return camera->hasDualStreaming();
            });

    ui->qualityDropdown->setEnabled(selectionHasDualStreamingDevices);

    ui->selectionDescriptionLabel->setText(
        tr("Set for %n selected devices", "", camerasIndexes.size()));
}

void BackupSettingsPickerWidget::loadState(const ServerSettingsDialogState& state)
{
    setupContentTypesDropdown(state.backupStoragesStatus.usesCloudBackupStorage);
}

void BackupSettingsPickerWidget::setupContentTypesDropdown(bool isCloudBackupStorage)
{
    m_contentTypesDropdownMenu = createContentTypesMenu(isCloudBackupStorage);
    connect(m_contentTypesDropdownMenu.get(), &QMenu::triggered, this,
        [this](QAction* action)
        {
            emit backupContentTypesPicked(static_cast<BackupContentTypes>(action->data().toInt()));
        });

    ui->contentsTypesDropdown->setMenu(m_contentTypesDropdownMenu.get());
}

void BackupSettingsPickerWidget::setupQualityDropdown()
{
    m_qualityDropdownMenu = createQualityMenu();
    connect(m_qualityDropdownMenu.get(), &QMenu::triggered, this,
        [this](QAction* action)
        {
            emit backupQualityPicked(static_cast<CameraBackupQuality>(action->data().toInt()));
        });

    ui->qualityDropdown->setMenu(m_qualityDropdownMenu.get());
}

void BackupSettingsPickerWidget::setupLayoutSyncWithHeaderView(const QHeaderView* headerView)
{
    connect(headerView, &QHeaderView::sectionResized, this,
        [this](int logicalIndex, int, int newSize)
        {
            if (logicalIndex == backup_settings_view::ContentTypesColumn)
                ui->contentsTypesDropdownContainer->setFixedWidth(newSize);

            if (logicalIndex == backup_settings_view::QualityColumn)
                ui->qualityDropdownContainer->setFixedWidth(newSize);
        });
}

} // namespace nx::vms::client::desktop
