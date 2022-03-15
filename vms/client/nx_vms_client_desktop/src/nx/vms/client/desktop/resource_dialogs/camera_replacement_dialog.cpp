// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_replacement_dialog.h"
#include "ui_camera_replacement_dialog.h"

#include <optional>

#include <QtGui/QCloseEvent>
#include <QtWidgets/QPushButton>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_selection_widget.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <utils/camera/camera_replacement.h>
#include <utils/common/event_processors.h>

namespace {

using namespace nx::vms::client::desktop;

static constexpr int kHeaderCaptionTextPixelSize = 24;
static constexpr auto kHeaderCaptionTextWeight = QFont::ExtraLight;

bool showServersInTree(QnWorkbenchContext* context)
{
    // No other checks, the dialog is accessible for administrators only.
    return context->resourceTreeSettings()->showServersInTree();
}

QString makeCameraNameRichText(const QString& cameraName, const QString& extraInfo = QString())
{
    using namespace nx::vms::common;

    static const auto kHighlightedNameTextColor = colorTheme()->color("light4");
    static const auto kExtraInfoTextColor = colorTheme()->color("dark17");

    const auto cameraNameRichText = html::bold(html::colored(cameraName, kHighlightedNameTextColor));
    const auto extraInfoRichText = html::colored(extraInfo, kExtraInfoTextColor);

    return extraInfo.isEmpty()
        ? cameraNameRichText
        : QStringList({cameraNameRichText, extraInfoRichText}).join(QChar::Nbsp);
}

// Removes and deletes all contents from the given layout.
void clearLayoutContents(QLayout* layout)
{
    while (auto layoutItem = layout->takeAt(0))
    {
        if (auto childLayout = layoutItem->layout())
        {
            clearLayoutContents(childLayout);
            childLayout->deleteLater();
        }
        if (auto widget = layoutItem->widget())
            widget->deleteLater();

        delete layoutItem;
    }
}

// Creates horizontal layout that contains 'success' icon following by the appropriately stylized
// caption text.
QLayout* createDataTransferSuccessItem(const QString& captionText)
{
    using namespace nx::vms::common;

    static const auto kCaptionTextColor = colorTheme()->color("light10");

    auto horizontalLayout = new QHBoxLayout();
    horizontalLayout->setContentsMargins({});
    horizontalLayout->setSpacing(nx::style::Metrics::kDefaultLayoutSpacing.width());

    auto successIconLabel = new QLabel();
    successIconLabel->setPixmap(qnSkin->pixmap("camera_replacement/success.svg"));

    auto captionLabel = new QLabel(html::bold(captionText));
    setPaletteColor(captionLabel, QPalette::WindowText, kCaptionTextColor);

    horizontalLayout->addWidget(successIconLabel, /*stretch*/ 0);
    horizontalLayout->addWidget(captionLabel, /*stretch*/ 1);

    return horizontalLayout;
}

// Creates grid layout that contains caption block with 'warning' icon and appropriately stylized
// caption text optionally following by number of separate lines with description messages.
QLayout* createDataTransferWarningItem(
    const QString& captionText,
    const QStringList& descriptionLines = QStringList())
{
    using namespace nx::vms::common;

    static constexpr auto kIconColumn = 0;
    static constexpr auto kTextColumn = 1;
    static constexpr auto kCaptionRow = 0;

    static const auto kCaptionTextColor = colorTheme()->color("yellow_core");
    static const auto kDescriptionTextColor = colorTheme()->color("yellow_d2");

    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins({});
    gridLayout->setHorizontalSpacing(nx::style::Metrics::kDefaultLayoutSpacing.width());
    gridLayout->setVerticalSpacing(nx::style::Metrics::kDefaultLayoutSpacing.height() / 2);

    auto warningIconLabel = new QLabel();
    warningIconLabel->setPixmap(qnSkin->pixmap("camera_replacement/warning.svg"));

    auto captionLabel = new QLabel(html::bold(captionText));
    setPaletteColor(captionLabel, QPalette::WindowText, kCaptionTextColor);

    gridLayout->addWidget(warningIconLabel, kCaptionRow, kIconColumn);
    gridLayout->addWidget(captionLabel, kCaptionRow, kTextColumn);
    gridLayout->setColumnStretch(kIconColumn, 0);
    gridLayout->setColumnStretch(kTextColumn, 1);

    for (const auto& descriptionLine: descriptionLines)
    {
        auto descriptionLabel = new QnWordWrappedLabel();
        descriptionLabel->setText(descriptionLine);
        setPaletteColor(descriptionLabel, QPalette::WindowText, kDescriptionTextColor);
        gridLayout->addWidget(descriptionLabel, gridLayout->rowCount() + 1, kTextColumn);
    }

    return gridLayout;
}

ResourceSelectionWidget::EntityFactoryFunction treeEntityCreationFunction(
    CameraReplacementDialog::SelectedCameraPurpose selectedCameraPurpose,
    bool showServersInTree)
{
    using namespace nx::vms::common::utils;

    DetailedResourceTreeWidget::ResourceFilter resourceFilter;
    switch (selectedCameraPurpose)
    {
        case CameraReplacementDialog::CameraToBeReplaced:
            resourceFilter = camera_replacement::cameraCanBeReplaced;
            break;

        case CameraReplacementDialog::ReplacementCamera:
            resourceFilter = camera_replacement::cameraCanBeUsedAsReplacement;
            break;
    };

    return
        [showServersInTree, resourceFilter]
        (const entity_resource_tree::ResourceTreeEntityBuilder* builder)
        {
            return builder->createDialogAllCamerasEntity(showServersInTree, resourceFilter);
        };
}

} // namespace

namespace nx::vms::client::desktop {

struct CameraReplacementDialog::Private
{
    CameraReplacementDialog* const q;

    SelectedCameraPurpose selectedCameraPurpose;
    QnVirtualCameraResourcePtr cameraToBeReplaced;
    QnVirtualCameraResourcePtr replacementCamera;
    ResourceSelectionWidget* resourceSelectionWidget;
    std::optional<nx::vms::api::DeviceReplacementResponse> deviceReplacementResponce;
};

CameraReplacementDialog::CameraReplacementDialog(
    SelectedCameraPurpose selectedCameraPurpose,
    const QnVirtualCameraResourcePtr& otherCamera,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraReplacementDialog),
    d(new Private{this})
{
    ui->setupUi(this);
    d->selectedCameraPurpose = selectedCameraPurpose;

    switch (d->selectedCameraPurpose)
    {
        case CameraToBeReplaced:
            d->replacementCamera = otherCamera;
            break;

        case ReplacementCamera:
            d->cameraToBeReplaced = otherCamera;
            break;
    }

    d->resourceSelectionWidget =
        new ResourceSelectionWidget(this, resource_selection_view::ColumnCount);
    ui->cameraSelectionPageLayout->addWidget(d->resourceSelectionWidget);

    d->resourceSelectionWidget->setShowRecordingIndicator(true);
    d->resourceSelectionWidget->setDetailsPanelHidden(true);
    d->resourceSelectionWidget->setSelectionMode(ResourceSelectionMode::ExclusiveSelection);
    d->resourceSelectionWidget->setTreeEntityFactoryFunction(
        treeEntityCreationFunction(d->selectedCameraPurpose, showServersInTree(context())));

    setupUiContols();
    resize(minimumSizeHint());
    updateButtons();
    updateHeader();

    connect(d->resourceSelectionWidget, &ResourceSelectionWidget::selectionChanged, this,
        [this]()
        {
            const auto selectedCamera = d->resourceSelectionWidget->selectedResource()
                .dynamicCast<QnVirtualCameraResource>();

            switch (d->selectedCameraPurpose)
            {
                case CameraToBeReplaced:
                    d->cameraToBeReplaced = selectedCamera;
                    break;

                case ReplacementCamera:
                    d->replacementCamera = selectedCamera;
                    break;
            };

            updateButtons();
        });

    connect(ui->nextButton, &QPushButton::clicked,
        this, &CameraReplacementDialog::onNextButtonClicked);

    connect(ui->backButton, &QPushButton::clicked,
        this, &CameraReplacementDialog::onBackButtonClicked);
}

CameraReplacementDialog::~CameraReplacementDialog()
{
}

bool CameraReplacementDialog::isEmpty() const
{
    return d->resourceSelectionWidget->isEmpty();
}

CameraReplacementDialog::DialogState CameraReplacementDialog::dialogState() const
{
    // TODO: #vbreus Invalid state should be returned in case if dialog was initialized with
    // invalid data or further camera replacement become impossible for some reason. Also,
    // corresponding visual state should be implemented.

    if (ui->stackedWidget->currentWidget() == ui->cameraSelectionPage)
        return CameraSelection;
    else if (ui->stackedWidget->currentWidget() == ui->dataTransferReportPage)
        return ReplacementApproval;
    else if (ui->stackedWidget->currentWidget() == ui->cameraReplacementSummaryPage)
        return ReplacementSummary;

    NX_ASSERT(false);
    return InvalidState;
}

int CameraReplacementDialog::exec()
{
    if (!NX_ASSERT(dialogState() != InvalidState))
        return QDialog::Rejected;

    return base_type::exec();
}

void CameraReplacementDialog::closeEvent(QCloseEvent* event)
{
    if (dialogState() == ReplacementSummary)
    {
        base_type::closeEvent(event);
        return;
    }

    const auto pressedButton = QnMessageBox::question(
        this,
        tr("Abort camera replacement?"),
        /*extras*/ {},
        QDialogButtonBox::Yes | QDialogButtonBox::No,
        QDialogButtonBox::No);

    event->setAccepted(pressedButton == QDialogButtonBox::Yes);
}

void CameraReplacementDialog::setDialogState(DialogState state)
{
    switch (state)
    {
        case CameraSelection:
            d->deviceReplacementResponce.reset();
            clearLayoutContents(ui->dataTransferContentsLayout);
            ui->stackedWidget->setCurrentWidget(ui->cameraSelectionPage);
            break;

        case ReplacementApproval:
            ui->stackedWidget->setCurrentWidget(ui->dataTransferReportPage);
            makeReplacementRequest(/*getReportOnly*/ true);
            break;

        case ReplacementSummary:
            ui->stackedWidget->setCurrentWidget(ui->cameraReplacementSummaryPage);
            updateReplacementSummaryPage();
            makeReplacementRequest(/*getReportOnly*/ false);
            break;

        default:
            NX_ASSERT(false);
            break;
    }

    updateButtons();
    updateHeader();
}

void CameraReplacementDialog::onNextButtonClicked()
{
    switch (dialogState())
    {
        case CameraSelection:
            setDialogState(ReplacementApproval);
            break;

        case ReplacementApproval:
            setDialogState(ReplacementSummary);
            break;

        case ReplacementSummary:
            accept();
            break;

        case InvalidState:
            reject();
            break;

        default:
            NX_ASSERT(false);
            break;
    };
}

void CameraReplacementDialog::onBackButtonClicked()
{
    switch (dialogState())
    {
        case ReplacementApproval:
            setDialogState(CameraSelection);
            break;

        default:
            NX_ASSERT(false);
            break;
    };
}

void CameraReplacementDialog::makeReplacementRequest(bool getReportOnly)
{
    using namespace nx::vms::api;

    const auto callback = nx::utils::guarded(this,
        [this, getReportOnly]
        (bool success, rest::Handle requestId, DeviceReplacementResponse replacementResponce)
        {
            if (success)
                d->deviceReplacementResponce = replacementResponce;
            else
                d->deviceReplacementResponce.reset();

            if (getReportOnly)
            {
                updateDataTransferReportPage();
                updateButtons();
            }
        });

        connectedServerApi()->replaceDevice(
            d->cameraToBeReplaced->getId(),
            d->replacementCamera->getPhysicalId(),
            getReportOnly,
            callback,
            thread());
}

void CameraReplacementDialog::setupUiContols()
{
    // Buttons appearance.
    setAccentStyle(ui->nextButton);

    // Data transfer report scroll area width constraint.
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    installEventHandler(ui->scrollArea, QEvent::Resize, this,
        [this] { ui->dataTransferReportWidget->setFixedWidth(ui->scrollArea->width()); });

    // Header appearance.
    static const auto kHeaderBackgroundColor = colorTheme()->color("dark8");
    static const auto kHeaderCaptionTextColor = colorTheme()->color("light4");
    static const auto kHeaderDetailsTextColor = colorTheme()->color("light10");

    setPaletteColor(ui->headerContentsWidget, QPalette::Window, kHeaderBackgroundColor);
    ui->headerContentsWidget->setAutoFillBackground(true);

    ui->headerCheckmarkLabel->setPixmap(qnSkin->pixmap("system_settings/update_checkmark.png"));

    QFont headerCaptionFont;
    headerCaptionFont.setPixelSize(kHeaderCaptionTextPixelSize);
    headerCaptionFont.setWeight(kHeaderCaptionTextPixelSize);
    ui->headerCaptionLabel->setFont(headerCaptionFont);
    setPaletteColor(ui->headerCaptionLabel, QPalette::WindowText, kHeaderCaptionTextColor);

    ui->headerDetailsLabel->label()->setAlignment(Qt::AlignCenter);
    setPaletteColor(ui->headerDetailsLabel, QPalette::WindowText, kHeaderDetailsTextColor);
}

void CameraReplacementDialog::updateDataTransferReportPage()
{
    clearLayoutContents(ui->dataTransferContentsLayout);

    if (!d->deviceReplacementResponce)
    {
        ui->dataTransferContentsLayout->addLayout(createDataTransferWarningItem(
            tr("Unable to replace camera")));
    }

    if (!d->deviceReplacementResponce->compatible)
    {
        ui->dataTransferContentsLayout->addLayout(createDataTransferWarningItem(
            tr("Unable to replace camera"), {tr("Cameras are not compatible")}));
    }
    else
    {
        // TODO: #vbreus All messages regarding data transfer, even predefined ones should come
        // from the server response for the consistency.
        ui->dataTransferContentsLayout->addLayout(createDataTransferSuccessItem(tr("Name")));
        ui->dataTransferContentsLayout->addLayout(createDataTransferSuccessItem(tr("Archive")));

        const auto report = d->deviceReplacementResponce->report;
        for (auto reportItr = report.cbegin(); reportItr != report.end(); ++reportItr)
        {
            QStringList warnings(reportItr->second.cbegin(), reportItr->second.cend());
            ui->dataTransferContentsLayout->addLayout(
                createDataTransferWarningItem(reportItr->first, warnings));
        }
    }

    ui->dataTransferContentsLayout->addStretch();
}

void CameraReplacementDialog::updateReplacementSummaryPage()
{
    // TODO: #vbreus Actual camera replacement request may be unsuccessful. Update implementation
    // as specification will be specification will be updated regarding such case.

    if (dialogState() != ReplacementSummary)
        return;

    static const auto kCaptionTextColor = colorTheme()->color("light10");
    static const auto kCameraPixmap = qnResIconCache->icon(QnResourceIconCache::Camera)
        .pixmap(nx::style::Metrics::kDefaultIconSize, QIcon::Selected);

    setPaletteColor(ui->replacedCameraCaptionLabel, QPalette::WindowText, kCaptionTextColor);
    setPaletteColor(ui->replacementCameraCaptionLabel, QPalette::WindowText, kCaptionTextColor);

    ui->replacedCameraIconLabel->setPixmap(kCameraPixmap);
    ui->replacedCameraNameLabel->setText(makeCameraNameRichText(
        d->cameraToBeReplaced->getName(),
        QnResourceDisplayInfo(d->replacementCamera).host()));

    ui->replacementCameraIconLabel->setPixmap(kCameraPixmap);
    ui->replacementCameraNameLabel->setText(makeCameraNameRichText(
        d->replacementCamera->getName()));
}

void CameraReplacementDialog::updateButtons()
{
    switch (dialogState())
    {
        case CameraSelection:
            ui->nextButton->setEnabled(!d->replacementCamera.isNull()
                && !d->cameraToBeReplaced.isNull());
            ui->nextButton->setText(tr("Next"));
            ui->backButton->setHidden(true);
            break;

        case ReplacementApproval:
            ui->nextButton->setEnabled(
                d->deviceReplacementResponce && d->deviceReplacementResponce->compatible);
            ui->nextButton->setText(tr("Next"));
            ui->backButton->setHidden(false);
            break;

        case ReplacementSummary:
        case InvalidState:
            ui->nextButton->setEnabled(true);
            ui->nextButton->setText(tr("Finish"));
            ui->backButton->setHidden(true);
            break;

        default:
            NX_ASSERT(false);
            break;
    }
}

void CameraReplacementDialog::updateHeader()
{
    switch (dialogState())
    {
        case CameraSelection:
            ui->headerCaptionLabel->setText(tr("Camera for Replacement"));
            switch (d->selectedCameraPurpose)
            {
                case CameraToBeReplaced:
                    //: %1 will be substituted with the camera's name.
                    ui->headerDetailsLabel->setText(
                        tr("Selected camera will be replaced by %1 and removed from the system")
                            .arg(makeCameraNameRichText(d->replacementCamera->getName())));
                    break;

                case ReplacementCamera:
                    //: %1 will be substituted with the camera's name.
                    ui->headerDetailsLabel->setText(
                        tr("%1 will be replaced by selected camera and removed from the system")
                            .arg(makeCameraNameRichText(d->cameraToBeReplaced->getName())));
                    break;
            }
            ui->headerDetailsLabel->setHidden(false);
            ui->headerCheckmarkLabel->setHidden(true);
            break;

        case ReplacementApproval:
            ui->headerCaptionLabel->setText(tr("Data for Transfer"));
            ui->headerDetailsLabel->setText(tr("Checking if the data from the camera can be "
                "transferred to the new one. Some data and settings may not supported for "
                "new Camera"));
            ui->headerDetailsLabel->setHidden(false);
            ui->headerCheckmarkLabel->setHidden(true);
            break;

        case ReplacementSummary:
            ui->headerCaptionLabel->setText(tr("Camera replaced!"));
            ui->headerDetailsLabel->setHidden(true);
            ui->headerCheckmarkLabel->setHidden(false);
            break;

        default:
            break;
    }
}

} // namespace nx::vms::client::desktop
