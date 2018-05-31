#include "camera_motion_settings_widget.h"
#include "ui_camera_motion_settings_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QButtonGroup>

#include <client_core/client_core_module.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/client/core/motion/helpers/camera_motion_helper.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/client/desktop/common/widgets/selectable_button.h>

namespace {

static constexpr QSize kSensitivityButtonSize(34, 34);
static constexpr qreal kSensitivityButtonOpacity = 0.3;

} // namespace

namespace nx {
namespace client {
namespace desktop {

CameraMotionSettingsWidget::CameraMotionSettingsWidget(
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraMotionSettingsWidget()),
    m_motionHelper(new core::CameraMotionHelper()),
    m_sensitivityButtons(new QButtonGroup(this)),
    m_motionView(new QQuickView(qnClientCoreModule->mainQmlEngine(), nullptr))
{
    ui->setupUi(this);
    ui->motionDetectionCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->motionDetectionCheckBox->setForegroundRole(QPalette::ButtonText);

    const auto sensitivityButtonPaintFunction =
        [this](QPainter* painter, const QStyleOption* option, const QWidget* widget) -> bool
        {
            const int index = m_sensitivityButtons->id((QAbstractButton*)widget);
            QColor color = index < m_sensitivityColors.size()
                ? m_sensitivityColors[index]
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
        button->setText(lit("%1").arg(i));
        button->setFixedSize(kSensitivityButtonSize);
        button->setCustomPaintFunction(sensitivityButtonPaintFunction);
        ui->motionSensitivityGroupBox->layout()->addWidget(button);
        m_sensitivityButtons->addButton(button, i);
    }

    connect(m_sensitivityButtons, QnButtonGroupIntButtonClicked, this,
        [this]()
        {
            if (auto motionItem = m_motionView->rootObject())
                motionItem->setProperty("currentSensitivity", m_sensitivityButtons->checkedId());
        });

    m_sensitivityButtons->setExclusive(true);
    m_sensitivityButtons->button(QnMotionRegion::kDefaultSensitivity)->setChecked(true);

    setHelpTopic(this, Qn::CameraSettings_Motion_Help);

    connect(m_motionView, &QQuickView::statusChanged, this,
        [this](QQuickView::Status status)
        {
            if (status != QQuickView::Status::Ready)
                return;

            auto motionItem = m_motionView->rootObject();
            motionItem->setProperty("cameraResourceId", m_cameraId);
            motionItem->setProperty("cameraMotionHelper", QVariant::fromValue(m_motionHelper.data()));
            motionItem->setProperty("currentSensitivity", m_sensitivityButtons->checkedId());
            motionItem->setProperty("sensitivityColors", QVariant::fromValue(m_sensitivityColors));
        });

    m_motionView->setSource(lit("Nx/Motion/MotionSettingsItem.qml"));
    m_motionView->setResizeMode(QQuickView::SizeRootObjectToView);
    ui->motionContainerWidget->layout()->addWidget(QWidget::createWindowContainer(m_motionView));

    connect(ui->motionDetectionCheckBox, &QCheckBox::toggled,
        store, &CameraSettingsDialogStore::setMotionDetectionEnabled);

    connect(m_motionHelper.data(), &core::CameraMotionHelper::motionRegionListChanged, this,
        [this, store]() { store->setMotionRegionList(m_motionHelper->motionRegionList()); });

    connect(ui->resetMotionRegionsButton, &QPushButton::clicked,
        this, &CameraMotionSettingsWidget::resetMotionRegions);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraMotionSettingsWidget::loadState);
}

CameraMotionSettingsWidget::~CameraMotionSettingsWidget()
{
}

void CameraMotionSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    m_cameraId = state.isSingleCamera() ? state.singleCameraProperties.id : QString();
    m_motionHelper->setMotionRegionList(state.singleCameraSettings.motionRegionList());

    ui->motionDetectionCheckBox->setChecked(state.hasMotion());

    ::setReadOnly(ui->motionDetectionCheckBox, state.readOnly);
    ::setReadOnly(ui->resetMotionRegionsButton, state.readOnly);

    for (auto button: m_sensitivityButtons->buttons())
        ::setReadOnly(button, state.readOnly);

    if (auto motionItem = m_motionView->rootObject())
    {
        motionItem->setProperty("cameraResourceId", m_cameraId);
        motionItem->setEnabled(!state.readOnly);
    }
}

QVector<QColor> CameraMotionSettingsWidget::sensitivityColors() const
{
    return m_sensitivityColors;
}

void CameraMotionSettingsWidget::setSensitivityColors(const QVector<QColor>& value)
{
    m_sensitivityColors = value;

    if (auto motionItem = m_motionView->rootObject())
        motionItem->setProperty("sensitivityColors", QVariant::fromValue(m_sensitivityColors));
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

} // namespace desktop
} // namespace client
} // namespace nx
