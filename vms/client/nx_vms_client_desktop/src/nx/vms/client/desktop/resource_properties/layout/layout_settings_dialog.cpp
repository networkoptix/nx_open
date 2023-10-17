// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtWidgets/QPushButton>

#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <ui/dialogs/common/custom_file_dialog.h>

#include "flux/layout_settings_dialog_state.h"
#include "flux/layout_settings_dialog_state_reducer.h"
#include "flux/layout_settings_dialog_store.h"
#include "watchers/layout_logical_ids_watcher.h"
#include "widgets/layout_background_settings_widget.h"
#include "widgets/layout_general_settings_widget.h"

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;
using BackgroundImageStatus = State::BackgroundImageStatus;

struct LayoutSettingsDialog::Private
{
    QPointer<LayoutSettingsDialogStore> store;
    QnLayoutResourcePtr layout;
    QPointer<LayoutBackgroundSettingsWidget> backgroundTab;
    std::unique_ptr<LayoutLogicalIdsWatcher> logicalIdsWatcher;

    void resetChanges()
    {
        store->loadLayout(layout);
    }

    void applyChanges()
    {
        const auto& state = store->state();

        layout->setLocked(state.locked);
        layout->setLogicalId(state.logicalId);
        layout->setFixedSize(state.fixedSizeEnabled ? state.fixedSize : QSize());

        const auto& background = state.background;
        if (background.supported)
        {
            layout->setBackgroundImageFilename(background.filename);
            // Do not save size change if no image was set.
            if (!background.filename.isEmpty())
            {
                layout->setBackgroundSize({background.width.value, background.height.value});
                layout->setBackgroundOpacity(0.01 * background.opacityPercent);
            }
        }
    }
};

LayoutSettingsDialog::LayoutSettingsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::LayoutSettingsDialog),
    d(new Private())
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);

    d->store = new LayoutSettingsDialogStore(this);

    auto generalTab = new LayoutGeneralSettingsWidget(d->store, ui->tabWidget);
    d->backgroundTab = new LayoutBackgroundSettingsWidget(d->store, ui->tabWidget);

    addPage((int) Tab::general, generalTab, tr("General"));
    addPage((int) Tab::background, d->backgroundTab, tr("Background"));

    connect(d->store, &LayoutSettingsDialogStore::stateChanged, this,
        &LayoutSettingsDialog::loadState);

    connect(d->backgroundTab, &LayoutBackgroundSettingsWidget::newImageUploadedSuccessfully, this,
        [this]
        {
            d->applyChanges();
            base_type::accept();
        });
}

LayoutSettingsDialog::~LayoutSettingsDialog()
{
}

bool LayoutSettingsDialog::setLayout(const QnLayoutResourcePtr& layout)
{
    d->logicalIdsWatcher.reset();
    d->layout = layout;

    if (layout && NX_ASSERT(layout->systemContext()))
    {
        d->logicalIdsWatcher = std::make_unique<LayoutLogicalIdsWatcher>(
            d->store.data(),
            layout,
            this);
    }

    d->resetChanges();
    return true;
}

void LayoutSettingsDialog::accept()
{
    const auto& background = d->store->state().background;
    if (!background.supported)
    {
        d->applyChanges();
        base_type::accept();
        return;
    }

    switch (background.status)
    {
        // If the image is not present or still not loaded then do nothing.
        case BackgroundImageStatus::empty:
        case BackgroundImageStatus::error:
            d->applyChanges();
            base_type::accept();
            return;

        // Current progress should be cancelled before accepting.
        case BackgroundImageStatus::downloading:
        case BackgroundImageStatus::loading:
        case BackgroundImageStatus::newImageLoading:
        case BackgroundImageStatus::newImageUploading:
            return;

        case BackgroundImageStatus::loaded:
            // Check if current image should be cropped and re-uploaded.
            if (background.canChangeAspectRatio() && background.cropToMonitorAspectRatio)
                break;
            appContext()->localSettings()->layoutKeepAspectRatio = background.keepImageAspectRatio;
            LayoutSettingsDialogStateReducer::setKeepBackgroundAspectRatio(
                appContext()->localSettings()->layoutKeepAspectRatio());
            d->applyChanges();
            base_type::accept();
            return;

        // Upload new image
        case BackgroundImageStatus::newImageLoaded:
            break;
    }
    d->backgroundTab->uploadImage();
}

void LayoutSettingsDialog::loadState(const LayoutSettingsDialogState& state)
{
    setPageVisible((int) Tab::background, state.background.supported);
    const bool isLoading = state.background.supported && state.background.loadingInProgress();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!isLoading);
}

} // namespace nx::vms::client::desktop
