#include "select_resources_button.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

namespace {

static const int kDefaultMaximumWidth = 300;

QIcon iconHelper(QnResourceIconCache::Key key)
{
    return QIcon(QnSkin::maximumSizePixmap(qnResIconCache->icon(key),
        QIcon::Selected, QIcon::Off, false));
}

} // namespace


QnSelectResourcesButton::QnSelectResourcesButton(QWidget* parent) :
    base_type(parent),
    m_singleSelectionParameters(true, true)
{
    setMaximumWidth(kDefaultMaximumWidth);
    setProperty(style::Properties::kPushButtonMargin, style::Metrics::kStandardPadding);
}

void QnSelectResourcesButton::selectAny()
{
    setAppearance(appearanceForAny());
}

void QnSelectResourcesButton::selectAll()
{
    setAppearance(appearanceForAll());
}

void QnSelectResourcesButton::selectOne(const QnResourcePtr& one)
{
    setAppearance(appearanceForResource(one));
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
    setIconSize(QnSkin::maximumSize(appearance.icon));
}

QnSelectResourcesButton::Appearance QnSelectResourcesButton::appearanceForResource(const QnResourcePtr& resource)
{
    if (!resource)
        return appearanceForSelected(1);

    auto key = qnResIconCache->key(resource);
    if (!m_singleSelectionParameters.showState)
        key &= QnResourceIconCache::TypeMask;

    return {
        iconHelper(key),
        m_singleSelectionParameters.showName
            ? (resource->getName())
            : appearanceForSelected(1).text };
}

QnSelectDevicesButton::QnSelectDevicesButton(QWidget* parent) :
    base_type(parent)
{
    setAppearance(appearanceForSelected(0));
}

QnSelectResourcesButton::Appearance QnSelectDevicesButton::appearanceForAny()
{
    return {
        iconHelper(QnResourceIconCache::Camera),
        QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("Any Device"),
            tr("Any Camera")) };
}

QnSelectResourcesButton::Appearance QnSelectDevicesButton::appearanceForAll()
{
    return {
        iconHelper(QnResourceIconCache::Cameras),
        QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("All Devices"),
            tr("All Cameras")) };
}

QnSelectResourcesButton::Appearance QnSelectDevicesButton::appearanceForSelected(int count)
{
    if (count == 0)
    {
        return {
            QIcon(),
            QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("Select devices..."),
                tr("Select cameras...")) };
    }

    return {
        iconHelper(count == 1 ? QnResourceIconCache::Camera : QnResourceIconCache::Cameras),
        QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("%n Devices", "", count),
            tr("%n Cameras", "", count)) };
}

QnSelectUsersButton::QnSelectUsersButton(QWidget* parent) :
    base_type(parent)
{
    setAppearance(appearanceForSelected(0));
}

QnSelectResourcesButton::Appearance QnSelectUsersButton::appearanceForAny()
{
    return { iconHelper(QnResourceIconCache::User), tr("Any User") };
}

QnSelectResourcesButton::Appearance QnSelectUsersButton::appearanceForAll()
{
    return { iconHelper(QnResourceIconCache::Users), tr("All Users") };
}

QnSelectResourcesButton::Appearance QnSelectUsersButton::appearanceForSelected(int count)
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
    setAppearance(appearanceForSelected(0));
}

QnSelectResourcesButton::Appearance QnSelectServersButton::appearanceForAny()
{
    return { iconHelper(QnResourceIconCache::Server), tr("Any Server") };
}

QnSelectResourcesButton::Appearance QnSelectServersButton::appearanceForAll()
{
    return { iconHelper(QnResourceIconCache::Servers), tr("All Servers") };
}

QnSelectResourcesButton::Appearance QnSelectServersButton::appearanceForSelected(int count)
{
    if (count == 0)
        return{ QIcon(), tr("Select Servers...") };

    return {
        iconHelper(count == 1 ? QnResourceIconCache::Server : QnResourceIconCache::Servers),
        tr("%n Servers", "", count) };
}
