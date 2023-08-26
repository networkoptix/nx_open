// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_motion_settings_widget.h"
#include "ui_camera_motion_settings_widget.h"

#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QButtonGroup>

#include <client_core/client_core_module.h>
#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/motion/helpers/camera_motion_helper.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/widgets/selectable_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/scoped_painter_rollback.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"
#include "private/motion_stream_alerts.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kSensitivityButtonSize(34, 34);
static constexpr qreal kSensitivityButtonOpacity = 0.3;

using StreamIndex = nx::vms::api::StreamIndex;

} // namespace

CameraMotionSettingsWidget::CameraMotionSettingsWidget(
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraMotionSettingsWidget()),
    m_motionHelper(new core::CameraMotionHelper()),
    m_sensitivityButtons(new QButtonGroup(this)),
    m_motionWidget(new QQuickWidget(qnClientCoreModule->mainQmlEngine(), this))
{
    ui->setupUi(this);
    ui->motionDetectionCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->motionDetectionCheckBox->setForegroundRole(QPalette::ButtonText);
    ui->highResolutionAlertBar->setRetainSpaceWhenNotDisplayed(true);

    const QList<QColor> sensitivityColors = core::colorTheme()->colors("camera.sensitivityColors");

    const auto sensitivityButtonPaintFunction =
        [this, sensitivityColors]
            (QPainter* painter, const QStyleOption* option, const QWidget* widget) -> bool
        {
            const int index = m_sensitivityButtons->id((QAbstractButton*)widget);
            QColor color = index < sensitivityColors.size()
                ? sensitivityColors[index]
                : palette().color(QPalette::WindowText);

            if (option->state.testFlag(QStyle::State_MouseOver))
                color = color.lighter(120);

            QnScopedPainterPenRollback penRollback(painter, color);
            QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
            if (index > 0)
            {
                color.setAlphaF(kSensitivityButtonOpacity);
                painter->setBrush(color);
            }

            painter->drawRect(core::Geometry::eroded(QRectF(option->rect), 0.5));

            if (auto button = qobject_cast<const QAbstractButton*>(widget))
                painter->drawText(option->rect, Qt::AlignCenter, button->text());

            return true;
        };

    for (int i = 0; i < QnMotionRegion::kSensitivityLevelCount; ++i)
    {
        auto button = new SelectableButton(ui->motionSensitivityGroupBox);
        button->setText(QString("%1").arg(i));
        button->setFixedSize(kSensitivityButtonSize);
        button->setCustomPaintFunction(sensitivityButtonPaintFunction);
        ui->motionSensitivityGroupBox->layout()->addWidget(button);
        m_sensitivityButtons->addButton(button, i);
    }

    connect(m_sensitivityButtons, QnButtonGroupButtonClicked, store,
        [this, store](QAbstractButton* button)
        {
            store->setCurrentMotionSensitivity(
                m_sensitivityButtons->buttons().indexOf(button));
        });

    m_sensitivityButtons->setExclusive(true);

    const auto model = new QStandardItemModel(this);
    ui->motionStreamComboBox->setModel(model);

    ui->motionStreamComboBox->addItem({}, tr("Primary Stream"),
        QVariant::fromValue(StreamIndex::primary));
    ui->motionStreamComboBox->addItem({}, tr("Secondary Stream"),
        QVariant::fromValue(StreamIndex::secondary));

    connect(ui->motionStreamComboBox, QnComboboxActivated, store,
        [comboBox = ui->motionStreamComboBox, store](int index)
        {
            store->setMotionStream(comboBox->itemData(index).value<StreamIndex>());
        });

    const auto forceDetectionButton = new QPushButton(ui->motionImplicitlyDisabledAlertBar);
    forceDetectionButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    forceDetectionButton->setText(tr("Force Motion Detection"));
    ui->motionImplicitlyDisabledAlertBar->verticalLayout()->addWidget(forceDetectionButton);

    check_box_utils::autoClearTristate(ui->motionDetectionCheckBox);

    connect(forceDetectionButton, &QPushButton::clicked, store,
        [store]() { store->forceMotionDetection(); });

    setHelpTopic(this, HelpTopic::Id::CameraSettings_Motion);

    connect(m_motionWidget, &QQuickWidget::statusChanged, this,
        [this, sensitivityColors](QQuickWidget::Status status)
        {
            if (status != QQuickWidget::Status::Ready)
                return;

            auto motionItem = this->motionItem();
            NX_ASSERT(motionItem);
            motionItem->setProperty("cameraResourceId", QVariant::fromValue(m_cameraId));
            motionItem->setProperty("cameraMotionHelper", QVariant::fromValue(m_motionHelper.data()));
            motionItem->setProperty("currentSensitivity", m_sensitivityButtons->checkedId());
            motionItem->setProperty("sensitivityColors", QVariant::fromValue(sensitivityColors));
            motionItem->setProperty("visible", QVariant::fromValue(false));
    });

    m_motionWidget->rootContext()->setContextProperty("maxTextureSize",
        QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE));

    m_motionWidget->setSource(QString("Nx/Motion/MotionSettingsItem.qml"));
    m_motionWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    ui->motionContainerWidget->layout()->addWidget(m_motionWidget);

    connect(ui->motionDetectionCheckBox, &QCheckBox::stateChanged, store,
        [store](int state)
        {
            store->setMotionDetectionEnabled(state == Qt::Checked);
        });

    connect(m_motionHelper.data(), &core::CameraMotionHelper::motionRegionListChanged, this,
        [this, store]() { store->setMotionRegionList(m_motionHelper->motionRegionList()); });

    connect(ui->resetMotionRegionsButton, &QPushButton::clicked,
        this, &CameraMotionSettingsWidget::resetMotionRegions);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraMotionSettingsWidget::loadState);

    ensurePolished();
    m_motionWidget->setClearColor(palette().color(QPalette::Window));
}

CameraMotionSettingsWidget::~CameraMotionSettingsWidget()
{
}

QQuickItem* CameraMotionSettingsWidget::motionItem() const
{
    return m_motionWidget->rootObject();
}

void CameraMotionSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    if (!state.isSingleCamera())
        return;

    m_cameraId = state.singleCameraId();
    m_motionHelper->setMotionRegionList(state.motion.regionList());

    const bool turnedOn = state.motion.enabled();
    check_box_utils::setupTristateCheckbox(
        ui->motionDetectionCheckBox,
        !state.isMotionImplicitlyDisabled(),
        turnedOn);

    ui->motionStreamWidget->setVisible(state.motion.supportsSoftwareDetection
        && state.devicesDescription.hasDualStreamingCapability == CombinedValue::All);

    const auto indexOf =
        [this](StreamIndex stream)
        {
            return ui->motionStreamComboBox->findData(QVariant::fromValue(stream));
        };

    ui->motionStreamComboBox->setCurrentIndex(indexOf(state.effectiveMotionStream()));
    ::setReadOnly(ui->motionStreamComboBox, state.readOnly);

    const auto model = qobject_cast<QStandardItemModel*>(ui->motionStreamComboBox->model());
    if (NX_ASSERT(model))
    {
        model->item(indexOf(StreamIndex::secondary))->setEnabled(
            !state.expert.dualStreamingDisabled());
    }

    ::setReadOnly(ui->motionDetectionCheckBox, state.readOnly);
    ::setReadOnly(ui->resetMotionRegionsButton, state.readOnly);
    ::setReadOnly(ui->motionStreamComboBox, state.readOnly);

    ui->sensitivitySettingsWidget->setEnabled(turnedOn);

    for (auto button: m_sensitivityButtons->buttons())
        ::setReadOnly(button, state.readOnly);

    m_sensitivityButtons->button(state.motion.currentSensitivity)->setChecked(true);

    if (auto motionItem = this->motionItem())
    {
        motionItem->setProperty("currentSensitivity", state.motion.currentSensitivity);
        motionItem->setProperty("cameraResourceId", QVariant::fromValue(m_cameraId));
        motionItem->setEnabled(turnedOn && !state.readOnly);
        motionItem->setOpacity(turnedOn ? 1.0 : style::Hints::kDisabledItemOpacity);

        // Always show the same stream which is actually used on the server side to make sure red
        // squares are displayed on correct places.
        motionItem->property("mediaPlayer").value<core::MediaPlayer*>()->setVideoQuality(
            state.motion.stream.getBase() == nx::vms::api::StreamIndex::primary
                ? core::MediaPlayer::HighVideoQuality
                : core::MediaPlayer::LowVideoQuality);
    }

    loadAlerts(state);
}

void CameraMotionSettingsWidget::loadAlerts(const CameraSettingsDialogState& state)
{
    const auto streamAlert = state.motion.streamAlert;
    const bool motionDetectionOn = state.motion.enabled()
        && !state.isMotionImplicitlyDisabled();

    ui->recordingAlertBar->setText(!motionDetectionOn || state.recording.enabled()
        ? QString()
        : tr("Motion detection will work only when camera is being viewed. "
            "Enable recording to make it work all the time."));

    ui->highResolutionAlertBar->setText(MotionStreamAlerts::resolutionAlert(streamAlert));
    ui->motionImplicitlyDisabledAlertBar->setText(MotionStreamAlerts::implicitlyDisabledAlert(
        streamAlert));

    ui->regionsAlertBar->setText(
        [&state]() -> QString
        {
            if (!state.motionAlert)
                return {};

            using MotionAlert = CameraSettingsDialogState::MotionAlert;
            switch (*state.motionAlert)
            {
                case MotionAlert::motionDetectionTooManyRectangles:
                    return tr("Maximum number of motion detection rectangles for current camera "
                        "is reached");

                case MotionAlert::motionDetectionTooManyMaskRectangles:
                    return tr("Maximum number of ignore motion rectangles for current camera "
                        "is reached");

                case MotionAlert::motionDetectionTooManySensitivityRectangles:
                    return tr("Maximum number of detect motion rectangles for current camera "
                        "is reached");
            }

            return {};
        }());

    ui->motionHintBar->setText(
        [&state]() -> QString
        {
            if (!state.motionHint)
                return QString();

            using MotionHint = CameraSettingsDialogState::MotionHint;
            switch (*state.motionHint)
            {
                case MotionHint::sensitivityChanged:
                    return tr("Select areas on the preview to set chosen sensitivity for.");

                case MotionHint::completelyMaskedOut:
                    return tr("Choose a motion detection sensitivity and select some areas "
                        "on the preview to set it for.");
            }

            return {};
        }());

    ui->highResolutionAlertBar->setRetainSpaceWhenNotDisplayed(
        ui->motionHintBar->text().isEmpty() &&
        ui->recordingAlertBar->text().isEmpty() &&
        ui->motionImplicitlyDisabledAlertBar->text().isEmpty() &&
        ui->regionsAlertBar->text().isEmpty());
}

void CameraMotionSettingsWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    if (auto motionItem = this->motionItem())
        motionItem->setProperty("visible", QVariant::fromValue(true));
}

void CameraMotionSettingsWidget::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);
    if (auto motionItem = this->motionItem())
        motionItem->setProperty("visible", QVariant::fromValue(false));
}

void CameraMotionSettingsWidget::resetMotionRegions()
{
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Reset motion regions to default?"),
        tr("This action cannot be undone."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        this);

    dialog.addCustomButton(QnMessageBoxCustomButton::Reset,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (dialog.exec() != QDialogButtonBox::Cancel)
        m_motionHelper->restoreDefaults();
}

} // namespace nx::vms::client::desktop
