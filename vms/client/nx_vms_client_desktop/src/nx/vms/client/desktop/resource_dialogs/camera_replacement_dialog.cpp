// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_replacement_dialog.h"
#include "ui_camera_replacement_dialog.h"

#include <optional>

#include <QtGui/QCloseEvent>
#include <QtWidgets/QPushButton>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_selection_widget.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <utils/camera/camera_replacement.h>

namespace {

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

constexpr int kDialogFixedWidth = 640;
constexpr int kHeaderCaptionTextPixelSize = 24;
constexpr auto kHeaderCaptionTextWeight = QFont::ExtraLight;

static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light17"}}},
};

static const QColor kLight4Color = "#E1E7EA";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kNormalIconSubstitutions = {
    {QIcon::Normal, {{kLight4Color, "light4"}}},
};

bool showServersInTree(const QnWorkbenchContext* context)
{
    // No other checks, the dialog is accessible for power users only.
    return context->resourceTreeSettings()->showServersInTree();
}

QString makeCameraNameRichText(const QString& cameraName, const QString& extraInfo = QString())
{
    using namespace nx::vms::common;

    static const auto kHighlightedNameTextColor = core::colorTheme()->color("light4");
    static const auto kExtraInfoTextColor = core::colorTheme()->color("light10");

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

// Creates grid layout that contains:
// - Caption row with an icon that corresponds to the given item type
// - Rows with additional description text lines
// All texts are stylized appropriately depending on the given item type as well.
QLayout* createDataTransferReportItem(
    const QString& captionText,
    const QStringList& descriptionTextLines,
    nx::vms::api::DeviceReplacementInfo::Level itemType)
{
    using namespace nx::vms::common;
    using namespace nx::vms::api;

    static constexpr auto kIconColumn = 0;
    static constexpr auto kTextColumn = 1;
    static constexpr auto kCaptionRow = 0;

    QColor captionTextColor;
    QColor descriptionTextColor;
    QPixmap iconPixmap;

    switch (itemType)
    {
        case DeviceReplacementInfo::Level::info:
            captionTextColor = core::colorTheme()->color("light10");
            descriptionTextColor = core::colorTheme()->color("light10");
            iconPixmap = qnSkin->pixmap("camera_replacement/success.svg");
            break;

        case DeviceReplacementInfo::Level::warning:
            captionTextColor = core::colorTheme()->color("yellow_core");
            descriptionTextColor = core::colorTheme()->color("yellow_d2");
            iconPixmap = qnSkin->pixmap("camera_replacement/warning.svg");
            break;

        case DeviceReplacementInfo::Level::error:
        case DeviceReplacementInfo::Level::critical:
            captionTextColor = core::colorTheme()->color("red_l2");
            descriptionTextColor = core::colorTheme()->color("red_l1");
            iconPixmap = qnSkin->pixmap("camera_replacement/error.svg");
            break;

        default:
            NX_ASSERT(false);
            break;
    }

    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins({});
    gridLayout->setHorizontalSpacing(nx::style::Metrics::kDefaultLayoutSpacing.width());
    gridLayout->setVerticalSpacing(nx::style::Metrics::kDefaultLayoutSpacing.height() / 2);

    auto warningIconLabel = new QLabel();
    warningIconLabel->setPixmap(iconPixmap);

    auto captionLabel = new QLabel(html::bold(captionText));
    setPaletteColor(captionLabel, QPalette::WindowText, captionTextColor);

    gridLayout->addWidget(warningIconLabel, kCaptionRow, kIconColumn);
    gridLayout->addWidget(captionLabel, kCaptionRow, kTextColumn);
    gridLayout->setColumnStretch(kIconColumn, 0);
    gridLayout->setColumnStretch(kTextColumn, 1);

    for (const auto& descriptionLine: descriptionTextLines)
    {
        auto descriptionLabel = new QnWordWrappedLabel();
        descriptionLabel->setText(descriptionLine);
        setPaletteColor(descriptionLabel, QPalette::WindowText, descriptionTextColor);
        gridLayout->addWidget(descriptionLabel, gridLayout->rowCount() + 1, kTextColumn);
    }

    return gridLayout;
}

ResourceSelectionWidget::EntityFactoryFunction treeEntityCreationFunction(
    const QnVirtualCameraResourcePtr& cameraToBeReplaced,
    bool showServersInTree)
{
    using namespace nx::vms::common::utils;

    return
        [cameraToBeReplaced, showServersInTree](
            const entity_resource_tree::ResourceTreeEntityBuilder* builder)
        {
            const auto resourceFilter =
                [cameraToBeReplaced](const QnResourcePtr& resource)
                {
                    return camera_replacement::cameraCanBeUsedAsReplacement(
                        cameraToBeReplaced, resource);
                };

            return builder->createDialogAllCamerasEntity(
                showServersInTree,
                resourceFilter);
        };
}

} // namespace

namespace nx::vms::client::desktop {

struct CameraReplacementDialog::Private
{
    CameraReplacementDialog* const q;

    QnVirtualCameraResourcePtr cameraToBeReplaced;
    QnVirtualCameraResourcePtr replacementCamera;
    ResourceSelectionWidget* resourceSelectionWidget;
    std::optional<nx::vms::api::DeviceReplacementResponse> deviceReplacementResponce;
    bool requestInProgress = false;

    bool hasCompatibleApiResponce() const;
    void reportFailureByMessageBox() const;
};

bool CameraReplacementDialog::Private::hasCompatibleApiResponce() const
{
    return deviceReplacementResponce && deviceReplacementResponce->compatible;
}

void CameraReplacementDialog::Private::reportFailureByMessageBox() const
{
    QString caption = tr("Failed to replace camera");
    QString message;

    if (deviceReplacementResponce && !deviceReplacementResponce->report.empty())
    {
        const auto reportItem = deviceReplacementResponce->report.front();
        caption = reportItem.name;
        if (!reportItem.messages.empty())
            message = *reportItem.messages.begin();
    }

    if (!deviceReplacementResponce)
    {
        message =
            tr("The Camera Replacement operation is not possible as the Server is unavailable.");
    }

    QnMessageBox::critical(
        q,
        caption,
        message,
        QDialogButtonBox::Ok,
        QDialogButtonBox::Ok);
}

CameraReplacementDialog::CameraReplacementDialog(
    const QnVirtualCameraResourcePtr& cameraToBeReplaced,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraReplacementDialog),
    d(new Private{.q=this, .cameraToBeReplaced=cameraToBeReplaced})
{
    ui->setupUi(this);
    setFixedWidth(kDialogFixedWidth);

    d->resourceSelectionWidget =
        new ResourceSelectionWidget(this, resource_selection_view::ColumnCount);
    ui->cameraSelectionPageLayout->addWidget(d->resourceSelectionWidget);

    d->resourceSelectionWidget->setShowRecordingIndicator(true);
    d->resourceSelectionWidget->setDetailsPanelHidden(true);
    d->resourceSelectionWidget->setSelectionMode(ResourceSelectionMode::ExclusiveSelection);
    d->resourceSelectionWidget->setTreeEntityFactoryFunction(
        treeEntityCreationFunction(cameraToBeReplaced, showServersInTree(context())));

    ui->refreshButton->setIcon(qnSkin->icon("text_buttons/reload_20.svg", kIconSubstitutions));

    setupUiContols();
    resize(minimumSizeHint());
    updateButtons();
    updateHeader();

    connect(d->resourceSelectionWidget, &ResourceSelectionWidget::selectionChanged, this,
        [this]()
        {
            d->replacementCamera = d->resourceSelectionWidget->selectedResource()
                .dynamicCast<QnVirtualCameraResource>();

            updateButtons();
        });

    connect(ui->nextButton, &QPushButton::clicked,
        this, &CameraReplacementDialog::onNextButtonClicked);

    connect(ui->backButton, &QPushButton::clicked,
        this, &CameraReplacementDialog::onBackButtonClicked);

    connect(ui->refreshButton, &QPushButton::clicked, this,
        [this]
        {
            d->resourceSelectionWidget->setSelectedResource({});
            d->resourceSelectionWidget->setTreeEntityFactoryFunction(
                treeEntityCreationFunction(d->cameraToBeReplaced, showServersInTree(context())));
            d->resourceSelectionWidget->resourceViewWidget()->makeRequiredItemsVisible();
        });

    setHelpTopic(this, HelpTopic::Id::CameraReplacementDialog);
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
    const auto failedDryRunRequest =
        dialogState() == ReplacementApproval
        && !d->requestInProgress
        && !d->deviceReplacementResponce;

    const auto incompatibleDryRunResponse =
        dialogState() == ReplacementApproval
        && d->deviceReplacementResponce
        && !d->deviceReplacementResponce->compatible;

    if (dialogState() == ReplacementSummary || failedDryRunRequest || incompatibleDryRunResponse)
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

void CameraReplacementDialog::onNextButtonClicked()
{
    switch (dialogState())
    {
        case CameraSelection:
            ui->stackedWidget->setCurrentWidget(ui->dataTransferReportPage);
            makeReplacementRequest(/*getReportOnly*/ true);
            break;

        case ReplacementApproval:
            updateReplacementSummaryPage();
            makeReplacementRequest(/*getReportOnly*/ false);
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

    updateButtons();
    updateHeader();
}

void CameraReplacementDialog::onBackButtonClicked()
{
    switch (dialogState())
    {
        case ReplacementApproval:
            d->deviceReplacementResponce.reset();
            clearLayoutContents(ui->dataTransferContentsLayout);
            ui->stackedWidget->setCurrentWidget(ui->cameraSelectionPage);
            break;

        default:
            NX_ASSERT(false);
            break;
    };

    updateButtons();
    updateHeader();
}

void CameraReplacementDialog::makeReplacementRequest(bool getReportOnly)
{
    using namespace nx::vms::api;

    if (!NX_ASSERT(!d->requestInProgress))
        return;

    const auto callback = nx::utils::guarded(this,
        [this, getReportOnly]
        (bool success, rest::Handle requestId, DeviceReplacementResponse replacementResponce)
        {
            d->requestInProgress = false;

            if (success)
                d->deviceReplacementResponce = replacementResponce;
            else
                d->deviceReplacementResponce.reset();

            if (getReportOnly)
            {
                if (d->hasCompatibleApiResponce())
                {
                    updateButtons();
                    updateDataTransferReportPage();
                }
                else
                {
                    d->reportFailureByMessageBox();
                    onBackButtonClicked(); //< Return to camera selection page.
                }
            }
            else
            {
                if (d->hasCompatibleApiResponce())
                {
                    ui->stackedWidget->setCurrentWidget(ui->cameraReplacementSummaryPage);
                    updateButtons();
                    updateHeader();
                }
                else
                {
                    d->reportFailureByMessageBox();
                    reject();
                }
            }
        });

        d->requestInProgress = true;

        const auto server = d->cameraToBeReplaced->getParentServer();
        server->restConnection()->replaceDevice(
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

    // Data transfer report scroll area properties.
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    // Header appearance.
    static const auto kHeaderBackgroundColor = core::colorTheme()->color("dark8");
    static const auto kHeaderCaptionTextColor = core::colorTheme()->color("light4");
    static const auto kHeaderDetailsTextColor = core::colorTheme()->color("light10");

    setPaletteColor(ui->headerContentsWidget, QPalette::Window, kHeaderBackgroundColor);
    ui->headerContentsWidget->setAutoFillBackground(true);

    ui->headerCheckmarkLabel->setPixmap(
        qnSkin->icon("system_settings/update_checkmark_40.svg", kNormalIconSubstitutions)
            .pixmap(QSize(40, 40)));

    QFont headerCaptionFont;
    headerCaptionFont.setPixelSize(kHeaderCaptionTextPixelSize);
    headerCaptionFont.setWeight(kHeaderCaptionTextWeight);
    ui->headerCaptionLabel->setFont(headerCaptionFont);
    setPaletteColor(ui->headerCaptionLabel, QPalette::WindowText, kHeaderCaptionTextColor);

    ui->headerDetailsLabel->label()->setAlignment(Qt::AlignCenter);
    setPaletteColor(ui->headerDetailsLabel, QPalette::WindowText, kHeaderDetailsTextColor);
}

void CameraReplacementDialog::updateDataTransferReportPage()
{
    using namespace nx::vms::api;

    clearLayoutContents(ui->dataTransferContentsLayout);

    if (!NX_ASSERT(d->deviceReplacementResponce && d->deviceReplacementResponce->compatible))
        return;

    const auto& report = d->deviceReplacementResponce.value().report;
    for (const auto& reportItem: report)
    {
        QStringList descriptionLines(reportItem.messages.cbegin(), reportItem.messages.cend());

        // Description messages from the report item are perpended with the dash prefix.
        for (auto& descriptionLine: descriptionLines)
            descriptionLine = QStringList(
                {{nx::UnicodeChars::kEmDash}, descriptionLine}).join(QChar::Space);

        // Additional sub caption message is added for detailed warning and error report items.
        if (!descriptionLines.empty())
        {
            if (reportItem.level == DeviceReplacementInfo::Level::warning)
                descriptionLines.push_front(tr("Will be transferred partially:"));

            if (reportItem.level == DeviceReplacementInfo::Level::error)
                descriptionLines.push_front(tr("Will not be transferred:"));
        }

        ui->dataTransferContentsLayout->addLayout(createDataTransferReportItem(
            reportItem.name,
            descriptionLines,
            reportItem.level));
    }

    ui->dataTransferContentsLayout->addStretch();
}

void CameraReplacementDialog::updateReplacementSummaryPage()
{
    static const auto kCaptionTextColor = core::colorTheme()->color("light10");
    static const auto kCameraPixmap = qnResIconCache->icon(QnResourceIconCache::Camera)
        .pixmap(nx::style::Metrics::kDefaultIconSize, QIcon::Selected);

    setPaletteColor(ui->replacedCameraCaptionLabel, QPalette::WindowText, kCaptionTextColor);
    setPaletteColor(ui->replacementCameraCaptionLabel, QPalette::WindowText, kCaptionTextColor);

    ui->replacementCameraIconLabel->setPixmap(kCameraPixmap);
    ui->replacementCameraNameLabel->setText(makeCameraNameRichText(
        d->replacementCamera->getName()));

    ui->replacedCameraNameLabel->setText(makeCameraNameRichText(
        d->cameraToBeReplaced->getName(),
        QnResourceDisplayInfo(d->replacementCamera).host()));
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
            ui->refreshButton->setHidden(false);
            break;

        case ReplacementApproval:
            ui->nextButton->setEnabled(d->deviceReplacementResponce
                && d->deviceReplacementResponce->compatible
                && !d->requestInProgress);
            ui->nextButton->setText(tr("Next"));
            ui->backButton->setEnabled(!d->requestInProgress);
            ui->backButton->setHidden(false);
            ui->refreshButton->setHidden(true);
            break;

        case ReplacementSummary:
        case InvalidState:
            ui->nextButton->setEnabled(true);
            ui->nextButton->setText(tr("Finish"));
            ui->backButton->setHidden(true);
            ui->refreshButton->setHidden(true);
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

            //: %1 will be substituted with the camera's name.
            ui->headerDetailsLabel->setText(
                tr("%1 will be removed from the System and replaced by the selected camera")
                    .arg(makeCameraNameRichText(d->cameraToBeReplaced->getName())));

            ui->headerDetailsLabel->setHidden(false);
            ui->headerCheckmarkLabel->setHidden(true);
            break;

        case ReplacementApproval:
            ui->headerCaptionLabel->setText(tr("Data for Transfer"));
            ui->headerDetailsLabel->setText(QStringList({
                tr("Checking if the old camera's data can be transferred to the new camera."),
                tr("Some data and settings may not be compatible with the new camera")})
                    .join(QChar::LineFeed));

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
