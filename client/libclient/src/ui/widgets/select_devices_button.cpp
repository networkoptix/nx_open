#include "select_devices_button.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>


QnSelectDevicesButton::QnSelectDevicesButton(QWidget* parent) :
    base_type(parent),
    m_mode(Selected),
    m_singleDeviceParameters()
{
    //TODO: #vkutin Refactor this property to something else in the future
    setProperty(style::Properties::kButtonMarginProperty, style::Metrics::kStandardPadding);
}

const QnVirtualCameraResourceList& QnSelectDevicesButton::selectedDevices() const
{
    return m_selectedDevices;
}

void QnSelectDevicesButton::setSelectedDevices(const QnVirtualCameraResourceList& list)
{
    m_selectedDevices = list;

    if (m_mode == Selected)
        updateData();
}

QnSelectDevicesButton::SingleDeviceParameters QnSelectDevicesButton::singleDeviceParameters() const
{
    return m_singleDeviceParameters;
}

void QnSelectDevicesButton::setSingleDeviceParameters(const SingleDeviceParameters& parameters)
{
    if (m_singleDeviceParameters == parameters)
        return;

    m_singleDeviceParameters = parameters;

    if (m_mode == Selected && m_selectedDevices.size() == 1)
        updateData();
}

QnSelectDevicesButton::Mode QnSelectDevicesButton::mode() const
{
    return m_mode;
}

void QnSelectDevicesButton::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    updateData();
}

void QnSelectDevicesButton::updateData()
{
    switch (m_mode)
    {
        case All:
        {
            QIcon icon = qnResIconCache->icon(QnResourceIconCache::Cameras);
            setIcon(QnSkin::maximumSizePixmap(icon, QIcon::Selected));
            setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("All Devices..."),
                tr("All Cameras...")));
            break;
        }

        case Any:
        {
            QIcon icon = qnResIconCache->icon(QnResourceIconCache::Camera);
            setIcon(QnSkin::maximumSizePixmap(icon, QIcon::Selected));
            setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("Any Device..."),
                tr("Any Camera...")));
            break;
        }

        case Selected:
        {
            switch (m_selectedDevices.size())
            {
                case 0:
                {
                    setIcon(QIcon());
                    setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                        tr("Select devices..."),
                        tr("Select cameras...")));
                    break;
                }

                case 1:
                {
                    QIcon icon = m_singleDeviceParameters.showState
                        ? qnResIconCache->icon(m_selectedDevices[0])
                        : qnResIconCache->icon(qnResIconCache->key(m_selectedDevices[0]) & QnResourceIconCache::TypeMask);

                    setIcon(QnSkin::maximumSizePixmap(icon, QIcon::Selected));
                    setText(m_singleDeviceParameters.showName
                        ? (m_selectedDevices[0]->getName() + lit("..."))
                        : QnDeviceDependentStrings::getDefaultNameFromSet(
                            tr("1 Device..."),
                            tr("1 Camera...")));
                    break;
                }

                default:
                {
                    QIcon icon = qnResIconCache->icon(QnResourceIconCache::Cameras);
                    setIcon(QnSkin::maximumSizePixmap(icon, QIcon::Selected));
                    setText(QnDeviceDependentStrings::getDefaultNameFromSet(
                        tr("%n Devices...", "", m_selectedDevices.size()),
                        tr("%n Cameras...", "", m_selectedDevices.size())));
                    break;
                }
            }

            break;
        }
    }
}

QnSelectDevicesButton::SingleDeviceParameters::SingleDeviceParameters(bool showName, bool showState) :
    showName(showName),
    showState(showState)
{
}

bool QnSelectDevicesButton::SingleDeviceParameters::operator == (const SingleDeviceParameters& other) const
{
    return showName == other.showName && showState == other.showState;
}
