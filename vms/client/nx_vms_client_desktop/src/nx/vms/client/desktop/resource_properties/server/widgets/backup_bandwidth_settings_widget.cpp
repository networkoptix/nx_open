// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_bandwidth_settings_widget.h"
#include "ui_backup_bandwidth_settings_widget.h"

#include <cstdint>

#include <QtCore/QSignalBlocker>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QGroupBox>

#include <core/resource/media_server_resource.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/types/day_of_week.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_properties/schedule/backup_bandwidth_schedule_cell_painter.h>
#include <nx/vms/common/utils/schedule.h>
#include <utils/common/event_processors.h>

namespace {

static constexpr int64_t kBitsInMegabit = 1000 * 1000;

enum BandwidthLimitType
{
    NoLimit,
    FixedLimit,
    ScheduledLimit,
};

using namespace nx::vms::client::desktop;

QVariant megabitsPerSecondToCellData(int megabitsPerSecond)
{
    return QVariant::fromValue<int64_t>(megabitsPerSecond * kBitsInMegabit / CHAR_BIT);
}

int bytesPerSecondToMegabitsPerSecond(int64_t bytesPerSecond)
{
    return bytesPerSecond * CHAR_BIT / kBitsInMegabit;
}

nx::vms::api::BackupBitrateBytesPerSecond getBackupBitrateLimitsFromGrid(
    const ScheduleGridWidget* scheduleGrid)
{
    nx::vms::api::BackupBitrateBytesPerSecond backupBitrateLimits;
    for (const auto dayOfWeek: nx::vms::common::daysOfWeek())
    {
        for (int hour = 0; hour < nx::vms::common::kHoursInDay; ++hour)
        {
            const auto cellData = scheduleGrid->cellData(dayOfWeek, hour);
            if (cellData.isNull())
                continue;

            backupBitrateLimits.insert(
                {nx::vms::api::dayOfWeek(dayOfWeek), hour}, cellData.value<int64_t>());
        }
    }
    return backupBitrateLimits;
}

void setBackupBitrateLimitsToGrid(
    BandwidthLimitType limitType,
    const nx::vms::api::BackupBitrateBytesPerSecond& bitrateLimits,
    ScheduleGridWidget* grid)
{
    if (limitType == NoLimit)
    {
        grid->resetGridData();
        return;
    }

    for (const auto dayOfWeek: nx::vms::common::daysOfWeek())
    {
        for (int hour = 0; hour < nx::vms::common::kHoursInDay; ++hour)
        {
            const nx::vms::api::BackupBitrateKey key = {nx::vms::api::dayOfWeek(dayOfWeek), hour};
            grid->setCellData(dayOfWeek, hour, bitrateLimits.contains(key)
                ? bitrateLimits.value(key)
                : QVariant());
        }
    }
}

BandwidthLimitType getBandwithLimitTypeFromBitrateLimits(
    const nx::vms::api::BackupBitrateBytesPerSecond& bitrateLimits)
{
    if (bitrateLimits.isEmpty())
        return NoLimit;

    if (bitrateLimits.size() != nx::vms::common::kDaysInWeek * nx::vms::common::kHoursInDay)
        return ScheduledLimit;

    const auto values = bitrateLimits.values();
    if (std::adjacent_find(values.cbegin(), values.cend(), std::not_equal_to<>()) == values.cend())
        return FixedLimit;

    return BandwidthLimitType::ScheduledLimit;
}

// Extension to the default buddy logic for brush selection buttons: clicking on the label not only
// transfers the keyboard focus to its buddy, but also triggers buddy click.
void buddyClickHandler(QObject* object, QEvent* event)
{
    if (!NX_ASSERT(event->type() == QEvent::MouseButtonRelease))
        return;

    const auto mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() != Qt::LeftButton)
        return;

    const auto label = qobject_cast<QLabel*>(object);
    if (!NX_ASSERT(label) || !label->rect().contains(mouseEvent->pos()))
        return;

    if (const auto brushButton = qobject_cast<SelectableButton*>(label->buddy()))
        brushButton->click();
}

void setFontWeight(QWidget* widget, int fontWeight)
{
    auto font = widget->font();
    font.setWeight((QFont::Weight) fontWeight);
    widget->setFont(font);
}

} // namespace

namespace nx::vms::client::desktop {

BackupBandwidthSettingsWidget::BackupBandwidthSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::BackupBandwidthSettingsWidget()),
    m_scheduleCellPainter(new BackupBandwidthScheduleCellPainter())
{
    ui->setupUi(this);
    ui->messageBar->init({.level = BarDescription::BarLevel::Info, .isClosable = false});

    for (const auto brushlabel: {ui->unlimitedLabel, ui->scheduledLimitLabel, ui->noBackupLabel})
        setFontWeight(brushlabel, QFont::Medium);

    ui->unlimitedButton->setCellPainter(m_scheduleCellPainter.get());
    ui->limitedButton->setCellPainter(m_scheduleCellPainter.get());
    ui->noBackupButton->setCellPainter(m_scheduleCellPainter.get());

    ui->unlimitedButton->setButtonBrush(QVariant());
    ui->limitedButton->setButtonBrush(megabitsPerSecondToCellData(
        ui->scheduledLimitSpinBox->value()));
    ui->noBackupButton->setButtonBrush(QVariant::fromValue<int64_t>(0));

    ui->scheduleGridWidget->setCellPainter(m_scheduleCellPainter.get());
    ui->scheduleGridWidget->setReadOnly(true);

    connect(ui->limitTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index)
        {
            ui->stackedWidget->setCurrentIndex(index);
            switch (index)
            {
                case NoLimit:
                    resetScheduleBrushButton();
                    ui->scheduleGridWidget->resetGridData();
                    break;

                case FixedLimit:
                    resetScheduleBrushButton();
                    ui->scheduleGridWidget->fillGridData(megabitsPerSecondToCellData(
                        ui->fixedLimitSpinbox->value()));
                    break;

                default:
                    break;
            }
        });

    connect(ui->fixedLimitSpinbox, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int value)
        {
            ui->scheduleGridWidget->fillGridData(megabitsPerSecondToCellData(value));
        });

    connect(ui->scheduledLimitSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int value)
        {
            ui->limitedButton->setButtonBrush(megabitsPerSecondToCellData(value));

            if (ui->buttonGroup->checkedButton() == ui->limitedButton)
                ui->scheduleGridWidget->setBrush(ui->limitedButton->buttonBrush());
        });

    connect(ui->buttonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonPressed),
        [this](QAbstractButton* button)
        {
            if (ui->buttonGroup->checkedButton() == button)
                ui->buttonGroup->setExclusive(false); //< Make possible to uncheck button.
        });

    connect(ui->buttonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonReleased),
        [this](QAbstractButton*) { ui->buttonGroup->setExclusive(true); });

    connect(ui->buttonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled),
        [this](QAbstractButton* button, bool checked)
        {
            if (button == ui->limitedButton)
                ui->scheduledLimitSpinBoxGroup->setEnabled(checked);

            const auto checkedButton = ui->buttonGroup->checkedButton();
            ui->scheduleGridWidget->setReadOnly(checkedButton == nullptr);

            if (auto brushButton = qobject_cast<ScheduleBrushSelectableButton*>(checkedButton))
                ui->scheduleGridWidget->setBrush(brushButton->buttonBrush());

            for (const auto brushlabel:
                {ui->unlimitedLabel, ui->scheduledLimitLabel, ui->noBackupLabel})
            {
                if (const auto button = qobject_cast<SelectableButton*>(brushlabel->buddy()))
                {
                    brushlabel->setForegroundRole(button->isChecked()
                        ? QPalette::Highlight
                        : QPalette::WindowText);
                }
            }

            updateBannerText();
        });

    connect(ui->scheduleGridWidget, &ScheduleGridWidget::gridDataChanged,
        this, &BackupBandwidthSettingsWidget::hasChangesChanged);

    connect(ui->scheduleGridWidget, &ScheduleGridWidget::gridDataEdited, this,
        [this]
        {
            m_showSheduledLimitEditHint = false;
            updateBannerText();
        });

    connect(ui->stackedWidget, &QStackedWidget::currentChanged,
        this, &BackupBandwidthSettingsWidget::updateBannerText);

    installEventHandler(ui->unlimitedLabel, QEvent::MouseButtonRelease, this, buddyClickHandler);
    installEventHandler(ui->scheduledLimitLabel, QEvent::MouseButtonRelease, this, buddyClickHandler);
    installEventHandler(ui->noBackupLabel, QEvent::MouseButtonRelease, this, buddyClickHandler);
}

BackupBandwidthSettingsWidget::~BackupBandwidthSettingsWidget()
{
}

bool BackupBandwidthSettingsWidget::hasChanges() const
{
    if (m_server.isNull())
        return false;

    return m_server->getBackupBitrateLimitsBps()
        != getBackupBitrateLimitsFromGrid(ui->scheduleGridWidget);
}

void BackupBandwidthSettingsWidget::loadDataToUi()
{
    if (m_server.isNull())
        return;

    const auto bitrateLimits = m_server->getBackupBitrateLimitsBps();

    const auto bandwidthLimitType = getBandwithLimitTypeFromBitrateLimits(bitrateLimits);
    {
        QSignalBlocker signalBlocker(ui->limitTypeComboBox);
        ui->limitTypeComboBox->setCurrentIndex(bandwidthLimitType);
        ui->stackedWidget->setCurrentIndex(bandwidthLimitType);
    }

    if (bandwidthLimitType == FixedLimit)
    {
        ui->fixedLimitSpinbox->setValue(bytesPerSecondToMegabitsPerSecond(
            bitrateLimits.cbegin().value()));
    }

    {
        QSignalBlocker signalBlocker(ui->scheduleGridWidget);
        setBackupBitrateLimitsToGrid(bandwidthLimitType, bitrateLimits, ui->scheduleGridWidget);
    }
    emit hasChangesChanged();
}

void BackupBandwidthSettingsWidget::applyChanges()
{
    const auto newBackupBitrateLimits = getBackupBitrateLimitsFromGrid(ui->scheduleGridWidget);
    m_server->setBackupBitrateLimitsBps(newBackupBitrateLimits);
}

void BackupBandwidthSettingsWidget::discardChanges()
{
    if (m_server)
        loadDataToUi();
    else
        ui->limitTypeComboBox->setCurrentIndex(NoLimit);
}

void BackupBandwidthSettingsWidget::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    if (m_server)
        m_server->disconnect(this);

    m_server = server;

    if (m_server)
    {
        connect(m_server.get(), &QnMediaServerResource::backupBitrateLimitsChanged,
            this, &BackupBandwidthSettingsWidget::loadDataToUi);
    }
}

void BackupBandwidthSettingsWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    resetScheduleBrushButton();
}

void BackupBandwidthSettingsWidget::updateBannerText()
{
    if (ui->limitTypeComboBox->currentIndex() != ScheduledLimit
        || !ui->buttonGroup->checkedButton()
        || !m_showSheduledLimitEditHint)
    {
        ui->messageBar->setText({});
        return;
    }

    ui->messageBar->setText(tr("Select an area on the schedule to apply chosen settings."));
}

void BackupBandwidthSettingsWidget::resetScheduleBrushButton()
{
    m_showSheduledLimitEditHint = true;
    if (const auto checkedButton = ui->buttonGroup->checkedButton())
    {
        ui->buttonGroup->setExclusive(false);
        checkedButton->setChecked(false);
        ui->buttonGroup->setExclusive(true);
    }
}

} // namespace nx::vms::client::desktop
