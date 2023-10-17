// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_virtual_joystick_dialog_mac.h"

#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/joystick/settings/device_hid.h>
#include <nx/vms/client/desktop/joystick/settings/osal/implementations/os_hid_device_virtual_mac.h>
#include <nx/vms/client/desktop/joystick/settings/osal/implementations/os_hid_driver_mac.h>
#include <nx/vms/client/desktop/joystick/settings/osal/osal_driver.h>
#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"
#include "virtual_joystick_dialog_mac.h"

namespace {

constexpr int kVirtualJoystickAxisMaximumValue = 1022;
constexpr int kVirtualJoystickReportMaxSize = 512;

} // namespace

namespace nx::vms::client::desktop {

joystick::OsHidDeviceVirtual* VirtualJoystickDialog::m_virtualJoystickDevice = nullptr;

VirtualJoystickDialog::VirtualJoystickDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::VirtualJoystickDialog()),
    m_virtualJoystickWidget(new QQuickWidget(appContext()->qmlEngine(), this))
{
    setWindowFlags(Qt::Dialog);

    ui->setupUi(this);
    ui->layout->addWidget(m_virtualJoystickWidget);

    m_virtualJoystickWidget->setClearColor(palette().window().color());
    m_virtualJoystickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    m_virtualJoystickWidget->setSource(QUrl("Nx/VirtualJoystick/VirtualJoystickDialog.qml"));

    QObject* rootObject = m_virtualJoystickWidget->rootObject();

    if (!NX_ASSERT(rootObject))
        return;

    connect(rootObject, SIGNAL(horizontalStickMoved(qreal)),
        this, SLOT(onHorizontalStickMoved(qreal)));
    connect(rootObject, SIGNAL(verticalStickMoved(qreal)),
        this, SLOT(onVerticalStickMoved(qreal)));
    connect(rootObject, SIGNAL(buttonPressed(int)), this, SLOT(onButtonPressed(int)));
    connect(rootObject, SIGNAL(buttonReleased(int)), this, SLOT(onButtonReleased(int)));

    attachVirtualJoystick();

    m_joystickControlsState = QBitArray(
        joystick::OsHidDeviceVirtual::joystickConfig()->reportSize.toInt());
}

VirtualJoystickDialog::~VirtualJoystickDialog()
{
    detachVirtualJoystick();
}

void VirtualJoystickDialog::registerAction()
{
    registerDebugAction(
        "Virtual Joystick",
        [](QnWorkbenchContext* context)
        {
            auto dialog = new VirtualJoystickDialog(context->mainWindowWidget());
            connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
            dialog->show();
        });
}

void VirtualJoystickDialog::onHorizontalStickMoved(qreal value)
{
    NX_ASSERT(value >= -0.5 && value <= 0.5);

    const auto bitLocations = joystick::DeviceHid::parseLocation(
        joystick::OsHidDeviceVirtual::joystickConfig()->xAxis.bits
    );

    NX_ASSERT(bitLocations.intervals.size() == 1);
    NX_ASSERT(bitLocations.size <= kVirtualJoystickReportMaxSize);

    const auto bitLocation = bitLocations.intervals[0];

    const quint16 decimalValue = kVirtualJoystickAxisMaximumValue * (0.5 + value);
    const std::bitset<kVirtualJoystickReportMaxSize> valueBits(decimalValue);

    for (int i = 0; i < bitLocations.size; ++i)
        m_joystickControlsState.setBit(i + bitLocation.first, valueBits[i]);

    setVirtualJoystickState(m_joystickControlsState);
}

void VirtualJoystickDialog::onVerticalStickMoved(qreal value)
{
    NX_ASSERT(value >= -0.5 && value <= 0.5);

    const auto bitLocations = joystick::DeviceHid::parseLocation(
        joystick::OsHidDeviceVirtual::joystickConfig()->yAxis.bits
    );

    NX_ASSERT(bitLocations.intervals.size() == 1);
    NX_ASSERT(bitLocations.size <= kVirtualJoystickReportMaxSize);

    const auto bitLocation = bitLocations.intervals[0];

    const quint16 decimalValue = kVirtualJoystickAxisMaximumValue * (0.5 + value);
    const std::bitset<kVirtualJoystickReportMaxSize> valueBits(decimalValue);

    for (int i = 0; i < bitLocations.size; ++i)
        m_joystickControlsState.setBit(i + bitLocation.first, valueBits[i]);

    setVirtualJoystickState(m_joystickControlsState);
}

void VirtualJoystickDialog::onButtonPressed(int buttonIndex)
{
    const int buttonShift = buttonShiftFromName(QString::number(buttonIndex));

    NX_ASSERT(buttonShift != -1);

    m_joystickControlsState.setBit(buttonShift, true);
    setVirtualJoystickState(m_joystickControlsState);
}

void VirtualJoystickDialog::onButtonReleased(int buttonIndex)
{
    const int buttonShift = buttonShiftFromName(QString::number(buttonIndex));

    NX_ASSERT(buttonShift != -1);

    m_joystickControlsState.setBit(buttonShift, false);
    setVirtualJoystickState(m_joystickControlsState);
}

int VirtualJoystickDialog::buttonShiftFromName(const QString& name) const
{
    const auto buttons = joystick::OsHidDeviceVirtual::joystickConfig()->buttons;

    for (const auto& button: buttons)
    {
        if (button.name != name)
            continue;

        const auto bitLocations = joystick::DeviceHid::parseLocation(button.bits);
        NX_ASSERT(bitLocations.size == 1);
        NX_ASSERT(bitLocations.intervals.size() == 1);
        NX_ASSERT(bitLocations.intervals[0].first == bitLocations.intervals[0].second);

        return bitLocations.intervals[0].first;
    }

    return -1;
}

void VirtualJoystickDialog::attachVirtualJoystick()
{
    auto driver = dynamic_cast<joystick::OsHidDriverMac*>(joystick::OsalDriver::getDriver());

    if (!NX_ASSERT(driver))
        return;

    if (!m_virtualJoystickDevice)
        m_virtualJoystickDevice = new joystick::OsHidDeviceVirtual();

    driver->attachVirtualDevice(m_virtualJoystickDevice);
}

void VirtualJoystickDialog::detachVirtualJoystick()
{
    auto driver = dynamic_cast<joystick::OsHidDriverMac*>(joystick::OsalDriver::getDriver());

    if (!NX_ASSERT(driver) || !NX_ASSERT(m_virtualJoystickDevice))
        return;

    driver->detachVirtualDevice(m_virtualJoystickDevice);
}

void VirtualJoystickDialog::setVirtualJoystickState(const QBitArray& newState)
{
    m_virtualJoystickDevice->setState(newState);
}

} // namespace nx::vms::client::desktop
