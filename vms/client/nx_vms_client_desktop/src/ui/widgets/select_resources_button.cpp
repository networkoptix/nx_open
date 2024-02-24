// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "select_resources_button.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

static const int kDefaultMaximumWidth = 300;

QIcon iconHelper(QnResourceIconCache::Key key)
{
    return QIcon(core::Skin::maximumSizePixmap(qnResIconCache->icon(key),
        QIcon::Selected, QIcon::Off, false));
}

} // namespace


QnSelectResourcesButton::QnSelectResourcesButton(QWidget* parent) :
    base_type(parent),
    m_singleSelectionParameters(true, true)
{
    setMaximumWidth(kDefaultMaximumWidth);
    setProperty(nx::style::Properties::kPushButtonMargin, nx::style::Metrics::kStandardPadding);
}

void QnSelectResourcesButton::selectAny()
{
    setAppearance(appearanceForAny());
}

void QnSelectResourcesButton::selectAll()
{
    setAppearance(appearanceForAll());
}

void QnSelectResourcesButton::selectNone()
{
    setAppearance(appearanceForSelected(0));
}

QnSelectResourcesButton::SingleSelectionParameters QnSelectResourcesButton::singleSelectionParameters() const
{
    return m_singleSelectionParameters;
}

void QnSelectResourcesButton::setSingleSelectionParameters(const SingleSelectionParameters& parameters)
{
    m_singleSelectionParameters = parameters;
}

QnSelectResourcesButton::SingleSelectionParameters::SingleSelectionParameters(bool showName, bool showState) :
    showName(showName),
    showState(showState)
{
}

bool QnSelectResourcesButton::SingleSelectionParameters::operator == (const SingleSelectionParameters& other) const
{
    return showName == other.showName && showState == other.showState;
}

void QnSelectResourcesButton::setAppearance(const Appearance& appearance)
{
    setText(appearance.text);
    setIcon(appearance.icon);
    setIconSize(core::Skin::maximumSize(appearance.icon));
}

QnSelectResourcesButton::Appearance QnSelectResourcesButton::appearanceForResource(const QnResourcePtr& resource) const
{
    if (!resource)
        return appearanceForSelected(0);

    auto key = qnResIconCache->key(resource);
    if (!m_singleSelectionParameters.showState)
        key &= QnResourceIconCache::TypeMask;

    return {
        iconHelper(key),
        m_singleSelectionParameters.showName
            ? (resource->getName())
            : appearanceForSelected(1).text };
}

QnSelectDevicesButton::QnSelectDevicesButton(QWidget* parent):
    base_type(parent)
{
    selectNone();
}

void QnSelectDevicesButton::selectDevices(const QnVirtualCameraResourceList& devices)
{
    selectList(devices);

    auto count = devices.size();
    if (count > 1 || (count == 1 && !singleSelectionParameters().showName))
    {
        auto systemContext = SystemContext::fromResource(devices.first());
        setText(QnDeviceDependentStrings::getNumericName(systemContext->resourcePool(), devices));
    }
}

QnSelectResourcesButton::Appearance QnSelectDevicesButton::appearanceForAny() const
{
    return {
        iconHelper(QnResourceIconCache::Camera),
        QnDeviceDependentStrings::getDefaultNameFromSet(
            appContext()->currentSystemContext()->resourcePool(),
            tr("Any Device"),
            tr("Any Camera")) };
}

QnSelectResourcesButton::Appearance QnSelectDevicesButton::appearanceForAll() const
{
    return {
        iconHelper(QnResourceIconCache::Cameras),
        QnDeviceDependentStrings::getDefaultNameFromSet(
            appContext()->currentSystemContext()->resourcePool(),
            tr("All Devices"),
            tr("All Cameras")) };
}

QnSelectResourcesButton::Appearance QnSelectDevicesButton::appearanceForSelected(int count) const
{
    if (count == 0)
    {
        return {
            QIcon(),
            QnDeviceDependentStrings::getDefaultNameFromSet(
                appContext()->currentSystemContext()->resourcePool(),
                tr("Select devices..."),
                tr("Select cameras...")) };
    }

    return {
        iconHelper(count == 1 ? QnResourceIconCache::Camera : QnResourceIconCache::Cameras),
        "<Internal Error>" };
}

QnSelectUsersButton::QnSelectUsersButton(QWidget* parent) :
    base_type(parent)
{
    selectNone();
}

void QnSelectUsersButton::selectUsers(const QnUserResourceList& users)
{
    selectList(users);
}

QnSelectResourcesButton::Appearance QnSelectUsersButton::appearanceForAny() const
{
    return { iconHelper(QnResourceIconCache::User), tr("Any User") };
}

QnSelectResourcesButton::Appearance QnSelectUsersButton::appearanceForAll() const
{
    return { iconHelper(QnResourceIconCache::Users), tr("All Users") };
}

QnSelectResourcesButton::Appearance QnSelectUsersButton::appearanceForSelected(int count) const
{
    if (count == 0)
        return{ QIcon(), tr("Select Users...") };

    return{
        iconHelper(count == 1 ? QnResourceIconCache::User : QnResourceIconCache::Users),
        tr("%n Users", "", count) };
}

QnSelectServersButton::QnSelectServersButton(QWidget* parent) :
    base_type(parent)
{
    selectNone();
}

void QnSelectServersButton::selectServers(const QnMediaServerResourceList& servers)
{
    selectList(servers);
}

QnSelectResourcesButton::Appearance QnSelectServersButton::appearanceForAny() const
{
    return { iconHelper(QnResourceIconCache::Server), tr("Any Server") };
}

QnSelectResourcesButton::Appearance QnSelectServersButton::appearanceForAll() const
{
    return { iconHelper(QnResourceIconCache::Servers), tr("All Servers") };
}

QnSelectResourcesButton::Appearance QnSelectServersButton::appearanceForSelected(int count) const
{
    if (count == 0)
        return{ QIcon(), tr("Select Servers...") };

    return {
        iconHelper(count == 1 ? QnResourceIconCache::Server : QnResourceIconCache::Servers),
        tr("%n Servers", "", count) };
}

QnSelectLayoutsButton::QnSelectLayoutsButton(QWidget* parent): QnSelectResourcesButton(parent)
{
}

void QnSelectLayoutsButton::selectLayouts(const QnLayoutResourceList& layouts)
{
    selectList(layouts);
}

QnSelectLayoutsButton::Appearance QnSelectLayoutsButton::appearanceForAny() const
{
    return { iconHelper(QnResourceIconCache::Layout), tr("Any Layout") };
}

QnSelectLayoutsButton::Appearance QnSelectLayoutsButton::appearanceForAll() const
{
    return { iconHelper(QnResourceIconCache::Layouts), tr("All Layouts") };
}

QnSelectLayoutsButton::Appearance QnSelectLayoutsButton::appearanceForSelected(int count) const
{
    if (count == 0)
        return{ QIcon(), tr("Select Layouts...") };

    return {
        iconHelper(count == 1 ? QnResourceIconCache::Layout : QnResourceIconCache::Layouts),
        tr("%n Layouts", "", count) };
}
