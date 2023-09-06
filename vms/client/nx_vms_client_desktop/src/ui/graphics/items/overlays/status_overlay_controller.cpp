// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "status_overlay_controller.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <nx/vms/client/core/skin/skin.h>
#include <utils/common/scoped_value_rollback.h>

namespace {

template<typename Type>
int toInt(Type value)
{
    return static_cast<int>(value);
}

template<typename Type, typename Container>
QString extractValue(Type index, const Container& container)
{
    const auto it = container.find(toInt(index));
    return (it == container.end() ? QString() : it.value());
}

bool isErrorOverlayCheck(Qn::ResourceStatusOverlay overlay)
{
    switch (overlay)
    {
        case Qn::EmptyOverlay:
        case Qn::LoadingOverlay:
        case Qn::NoDataOverlay:
        case Qn::NoVideoDataOverlay:
        case Qn::NoLiveStreamOverlay:
            return false;
        default:
            return true;
    }
}

} // unnamed namespace

QnStatusOverlayController::QnStatusOverlayController(
    const QnResourcePtr& resource,
    const StatusOverlayWidgetPtr& widget,
    QObject* parent)
    :
    base_type(parent),
    m_widget(widget),
    m_buttonTexts(getButtonCaptions(resource)),
    m_visibleItems( QnStatusOverlayWidget::Control::kNoControl),
    m_statusOverlay(Qn::EmptyOverlay),
    m_currentButton(Qn::ResourceOverlayButton::Empty),

    m_isErrorOverlay(isErrorOverlayCheck(m_statusOverlay))
{
    NX_ASSERT(resource);
    NX_ASSERT(m_widget, "Status overlay widget can't be nullptr");
    if (!m_widget)
        return;

    connect(this, &QnStatusOverlayController::statusOverlayChanged,
        this, &QnStatusOverlayController::onStatusOverlayChanged);
    connect(this, &QnStatusOverlayController::currentButtonChanged,
        this, &QnStatusOverlayController::updateVisibleItems);

    connect(m_widget, &QnStatusOverlayWidget::actionButtonClicked, this,
        [this] { emit buttonClicked(m_currentButton); });
    connect(m_widget, &QnStatusOverlayWidget::customButtonClicked,
        this, &QnStatusOverlayController::customButtonClicked);

    connect(this, &QnStatusOverlayController::isErrorOverlayChanged, this,
        [this]() { m_widget->setErrorStyle(isErrorOverlay()); });

    connect(this, &QnStatusOverlayController::currentButtonChanged, this,
        [this]() { m_widget->setButtonText(currentButtonText()); });

    connect(this, &QnStatusOverlayController::customButtonTextChanged, this,
        [this]()
        {
            updateVisibleItems();
            m_widget->setCustomButtonText(customButtonText());
        });

    connect(this, &QnStatusOverlayController::visibleItemsChanged,
        this, &QnStatusOverlayController::updateWidgetItems);
}

QnStatusOverlayWidget::Controls QnStatusOverlayController::visibleItems() const
{
    return m_visibleItems;
}

void QnStatusOverlayController::updateVisibleItems()
{
    const auto items = (isErrorOverlayCheck(statusOverlay())
        ? errorVisibleItems() : normalVisibleItems());

    if (m_visibleItems == items)
        return;

    m_visibleItems = items;
    emit visibleItemsChanged();
}

void QnStatusOverlayController::updateWidgetItems()
{
    NX_ASSERT(m_widget, "Status overlay widget can't be nullptr");
    if (!m_widget)
        return;

    auto items = visibleItems();
    m_widget->setVisibleControls(items);

    m_widget->setIconOverlayPixmap(statusIcon(statusOverlay()));
}

void QnStatusOverlayController::onStatusOverlayChanged(bool /*animated*/)
{
    NX_ASSERT(m_widget, "Status overlay widget can't be nullptr");
    if (!m_widget)
        return;

    const auto isVisibleToParent =
        [](const QGraphicsWidget* widget)
        {
            return widget->isVisibleTo(widget->parentItem());
        };

    /* As graphics widgets don't have "setUpdatesEnabled",
       temporarily make the widget invisible instead: */
    QnScopedTypedPropertyRollback<bool, QGraphicsWidget> visibilityRollback(
        m_widget.data(), &QGraphicsWidget::setVisible, isVisibleToParent, false);

    m_widget->setCaption(captionText(m_statusOverlay));
    m_widget->setDescription(descriptionText(m_statusOverlay));
    m_widget->setSuggestion(
        suggestionText(m_statusOverlay, m_currentButton != Qn::ResourceOverlayButton::Empty));

    m_widget->setIcon(statusIcon(m_statusOverlay));

    updateErrorState();
    updateVisibleItems();
}

QnStatusOverlayWidget::Controls QnStatusOverlayController::errorVisibleItems() const
{
    const auto overlay = statusOverlay();

    QnStatusOverlayWidget::Controls result = QnStatusOverlayWidget::Control::kCaption;

    if (!statusIconPath(overlay).isEmpty())
        result |= QnStatusOverlayWidget::Control::kIcon;

    const bool buttonPresent = m_currentButton != Qn::ResourceOverlayButton::Empty;
    if (buttonPresent)
        result |= QnStatusOverlayWidget::Control::kButton;

    if (!descriptionText(overlay).isEmpty())
        result |= QnStatusOverlayWidget::Control::kDescription;

    if (!suggestionText(overlay, buttonPresent).isEmpty())
        result |= QnStatusOverlayWidget::Control::kSuggestion;

    if (!m_customButtonText.isEmpty())
        result |= QnStatusOverlayWidget::Control::kCustomButton;

    return result;
}

QnStatusOverlayWidget::Controls QnStatusOverlayController::normalVisibleItems() const
{
    switch (statusOverlay())
    {
        case Qn::LoadingOverlay:
            return QnStatusOverlayWidget::Control::kPreloader;
        case Qn::NoVideoDataOverlay:
            return QnStatusOverlayWidget::Control::kImageOverlay;
        case Qn::NoDataOverlay:
        case Qn::NoLiveStreamOverlay:
            return QnStatusOverlayWidget::Control::kCaption;
        default:
            return QnStatusOverlayWidget::Control::kNoControl;
    }
}

Qn::ResourceStatusOverlay QnStatusOverlayController::statusOverlay() const
{
    return m_statusOverlay;
}

void QnStatusOverlayController::setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay,
    bool animated)
{
    if (m_statusOverlay == statusOverlay)
        return;

    m_statusOverlay = statusOverlay;
    emit statusOverlayChanged(animated);
}

QString QnStatusOverlayController::customButtonText() const
{
    return m_customButtonText;
}

void QnStatusOverlayController::setCustomButtonText(const QString& text)
{
    if (m_customButtonText == text)
        return;

    m_customButtonText = text;
    emit customButtonTextChanged();
}

bool QnStatusOverlayController::isErrorOverlay() const
{
    return m_isErrorOverlay;
}

void QnStatusOverlayController::updateErrorState()
{
    const bool isError = isErrorOverlayCheck(statusOverlay());
    if (m_isErrorOverlay == isError)
        return;

    m_isErrorOverlay = isError;
    emit isErrorOverlayChanged();
}

Qn::ResourceOverlayButton QnStatusOverlayController::currentButton() const
{
    return m_currentButton;
}

void QnStatusOverlayController::setCurrentButton(Qn::ResourceOverlayButton button)
{
    if (m_currentButton == button)
        return;

    m_currentButton = button;
    emit currentButtonChanged();
}

QString QnStatusOverlayController::currentButtonText() const
{
    return extractValue(currentButton(), m_buttonTexts);
}

QString QnStatusOverlayController::captionText(Qn::ResourceStatusOverlay overlay)
{
    static const auto kNotEnoughLicenses = tr("NOT ENOUGH LICENSES");
    static const IntStringHash kCaptions
    {
        { Qn::NoDataOverlay, tr("NO DATA") },
        { Qn::UnauthorizedOverlay, tr("UNAUTHORIZED") },
        { Qn::AccessDeniedOverlay, tr("NO ACCESS") },
        { Qn::NoExportPermissionOverlay, tr("NO EXPORT PERMISSION") },
        { Qn::OfflineOverlay, tr("OFFLINE") },
        { Qn::AnalogWithoutLicenseOverlay, kNotEnoughLicenses },
        { Qn::VideowallWithoutLicenseOverlay, kNotEnoughLicenses },
        { Qn::ServerOfflineOverlay, tr("SERVER UNAVAILABLE") },
        { Qn::ServerUnauthorizedOverlay, tr("NO ACCESS") },
        { Qn::IoModuleDisabledOverlay, tr("DEVICE DISABLED") },
        { Qn::TooManyOpenedConnectionsOverlay, tr("TOO MANY CONNECTIONS") },
        { Qn::PasswordRequiredOverlay, tr("PASSWORD REQUIRED") },
        { Qn::NoLiveStreamOverlay, tr("NO LIVE STREAM") },
        { Qn::OldFirmwareOverlay, tr("UNSUPPORTED FIRMWARE VERSION") },
        { Qn::CannotDecryptMediaOverlay, tr("ARCHIVE ENCRYPTED") },
        { Qn::InformationRequiredOverlay, tr("INFORMATION REQUIRED") },
        { Qn::SaasSuspended, tr("SAAS SUSPENDED") },
        { Qn::SaasShutDown, tr("SAAS SHUT DOWN") },
    };
    return extractValue(overlay, kCaptions);
}

QString QnStatusOverlayController::descriptionText(Qn::ResourceStatusOverlay overlay)
{
    switch (overlay)
    {
        case Qn::UnauthorizedOverlay:
            return tr("Please check authentication information");
        default:
            break;
    }
    return QString();
}

QString QnStatusOverlayController::suggestionText(Qn::ResourceStatusOverlay overlay,
    bool buttonPresent)
{
    switch (overlay)
    {
        case Qn::CannotDecryptMediaOverlay:
            if (buttonPresent)
                return QString();

            return tr("Ask your system administrator to enter the encryption password to decrypt this archive");

        default:
            break;
    }
    return QString();
}

QString QnStatusOverlayController::statusIconPath(Qn::ResourceStatusOverlay overlay)
{
    static const auto kLicenceIconPath = lit("item_placeholders/license.png");
    static const IntStringHash kIconPaths
    {
        { Qn::UnauthorizedOverlay, "item_placeholders/unauthorized.png" },
        { Qn::AccessDeniedOverlay, "item_placeholders/no_access.svg" },
        { Qn::NoExportPermissionOverlay, "item_placeholders/no_access.svg" },
        { Qn::OfflineOverlay, "item_placeholders/offline.svg" },
        { Qn::AnalogWithoutLicenseOverlay, kLicenceIconPath },
        { Qn::VideowallWithoutLicenseOverlay, kLicenceIconPath },
        { Qn::ServerUnauthorizedOverlay, "item_placeholders/no_access.svg" },
        { Qn::IoModuleDisabledOverlay, "item_placeholders/disabled.png" },
        { Qn::NoVideoDataOverlay, "item_placeholders/sound.png" },
        { Qn::PasswordRequiredOverlay, "item_placeholders/alert.png" },
        { Qn::CannotDecryptMediaOverlay, "item_placeholders/encrypted.svg" },
        { Qn::InformationRequiredOverlay, "item_placeholders/alert.png" },
        { Qn::SaasSuspended, "item_placeholders/offline.svg" }, //< TODO: #vbreus Change icon if needed.
        { Qn::SaasShutDown, "item_placeholders/offline.png" },
    };

    return extractValue(overlay, kIconPaths);
}

QPixmap QnStatusOverlayController::statusIcon(Qn::ResourceStatusOverlay status)
{
    const auto pixmapPath = statusIconPath(status);
    return pixmapPath.isEmpty() ? QPixmap() : qnSkin->pixmap(pixmapPath, true);
}

QnStatusOverlayController::IntStringHash
QnStatusOverlayController::getButtonCaptions(const QnResourcePtr& resource)
{
    NX_ASSERT(resource);

    const auto settingNameSet = QnCameraDeviceStringSet(
        tr("Device Settings"),
        tr("Camera Settings"),
        tr("I/O Module Settings"));

    const auto camera = (resource
        ? resource.dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr());
    IntStringHash result;
    result.insert(toInt(Qn::ResourceOverlayButton::Diagnostics), tr("Diagnostics"));
    result.insert(toInt(Qn::ResourceOverlayButton::EnableLicense), tr("Enable"));
    result.insert(toInt(Qn::ResourceOverlayButton::MoreLicenses), tr("Activate License"));
    result.insert(toInt(Qn::ResourceOverlayButton::SetPassword), tr("Set for this Camera"));
    result.insert(toInt(Qn::ResourceOverlayButton::UnlockEncryptedArchive), tr("Unlock"));
    result.insert(toInt(Qn::ResourceOverlayButton::RequestInformation), tr("Provide"));

    if (resource)
    {
        result.insert(toInt(Qn::ResourceOverlayButton::Settings),
            QnDeviceDependentStrings::getNameFromSet(
                resource->resourcePool(),
                settingNameSet,
                camera));
    }
    return result;
}
