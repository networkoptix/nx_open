#include "layout_settings_dialog.h"
#include "ui_layout_settings_dialog.h"

#include <QtWidgets/QPushButton>

#include <client/client_settings.h>
#include <core/resource/layout_resource.h>

#include <ui/dialogs/common/custom_file_dialog.h>

#include "redux/layout_settings_dialog_state.h"
#include "redux/layout_settings_dialog_state_reducer.h"
#include "redux/layout_settings_dialog_store.h"

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
        layout->setBackgroundImageFilename(background.filename);
        // Do not save size change if no image was set.
        if (!background.filename.isEmpty())
        {
            layout->setBackgroundSize({background.width.value, background.height.value});
            layout->setBackgroundOpacity(0.01 * background.opacityPercent);
        }
    }
};

LayoutSettingsDialog::LayoutSettingsDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::LayoutSettingsDialog),
    d(new Private())
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);

    d->store = new LayoutSettingsDialogStore(this);
    connect(d->store, &LayoutSettingsDialogStore::stateChanged, this,
        &LayoutSettingsDialog::loadState);

    auto generalTab = new LayoutGeneralSettingsWidget(d->store, ui->tabWidget);
    d->backgroundTab = new LayoutBackgroundSettingsWidget(d->store, ui->tabWidget);

    connect(d->backgroundTab, &LayoutBackgroundSettingsWidget::newImageUploadedSuccessfully, this,
        [this]
        {
            d->applyChanges();
            base_type::accept();
        });

    addPage(
        0,
        generalTab,
        tr("General"));

    addPage(
        1,
        d->backgroundTab,
        tr("Background"));
}

LayoutSettingsDialog::~LayoutSettingsDialog()
{
}

bool LayoutSettingsDialog::setLayout(const QnLayoutResourcePtr& layout)
{
    d->layout = layout;
    d->resetChanges();
    return true;
}

void LayoutSettingsDialog::accept()
{
    const auto& background = d->store->state().background;

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
            qnSettings->setLayoutKeepAspectRatio(background.keepImageAspectRatio);
            LayoutSettingsDialogStateReducer::setKeepBackgroundAspectRatio(
                qnSettings->layoutKeepAspectRatio());
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
    const bool isLoading = state.background.loadingInProgress();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!isLoading);
}

} // namespace nx::vms::client::desktop
