// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "status_overlay_controller.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <nx/vms/client/core/skin/skin.h>
#include <utils/common/scoped_value_rollback.h>

#include "client/client_globals.h"

namespace {

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kRedTheme = {{QIcon::Normal, {.primary = "red"}}};
const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kWhiteTheme = {{QIcon::Normal, {.primary = "light16"}}};

static const auto kLockIconPath = "48x48/Outline/lock.svg";
static const auto kRestrictIconPath = "48x48/Outline/restrict.svg";
static const auto kErrorIconPath = "48x48/Outline/error.svg";
static const auto kSoundIconPath = "48x48/Outline/sound.svg";
static const auto kPaidIconPath = "48x48/Outline/paid.svg";

struct OverlayInfo
{
    QnStatusOverlayWidget::ErrorStyle style;
    QString caption;
    QString statusIconPath;
    QString tooltip;
};

using OverlayInfoMap = std::unordered_map<Qn::ResourceStatusOverlay, OverlayInfo>;

const OverlayInfoMap& overlayInfo()
{
    static const QString kNotEnoughLicenses = QnStatusOverlayWidget::tr("NOT ENOUGH LICENSES");

    static const OverlayInfoMap kOverlayInfo = {
        {Qn::EmptyOverlay, {QnStatusOverlayWidget::ErrorStyle::red}},
        {Qn::LoadingOverlay, {QnStatusOverlayWidget::ErrorStyle::red}},
        {Qn::OfflineOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("OFFLINE"),
                kErrorIconPath,
                QnStatusOverlayWidget::tr(
                    "This camera cannot be accessed. Perform camera diagnostics within the Desktop Client for additional information.")}},
        {Qn::ServerOfflineOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("OFFLINE"),
                kErrorIconPath}},
        {Qn::UnauthorizedOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("UNAUTHORIZED"),
                kLockIconPath,
                QnStatusOverlayWidget::tr(
                    "This camera requires authorized credentials to be set in the device settings in Cloud portal or Desktop client.")}},
        {Qn::ServerUnauthorizedOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("UNAUTHORIZED"),
                kLockIconPath}},
        {Qn::AnalogWithoutLicenseOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red, kNotEnoughLicenses, kLockIconPath}},
        {Qn::VideowallWithoutLicenseOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red, kNotEnoughLicenses, kLockIconPath}},
        {Qn::IoModuleDisabledOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red, kNotEnoughLicenses, kLockIconPath}},
        {Qn::OldFirmwareOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("UNSUPPORTED"),
                kLockIconPath}},
        {Qn::PasswordRequiredOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("PASSWORD REQUIRED"),
                kLockIconPath}},
        {Qn::SaasShutDown,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("SITE SHUT DOWN"),
                kErrorIconPath}},
        {Qn::RestrictedOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("STREAM LIMITATION"),
                kPaidIconPath}},
        {Qn::InformationRequiredOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white,
                QnStatusOverlayWidget::tr("INFORMATION REQUIRED"),
                kRestrictIconPath}},
        {Qn::NoVideoDataOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white,
                QnStatusOverlayWidget::tr("AUDIO ONLY"),
                kSoundIconPath}},
        {Qn::NoDataOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white, QnStatusOverlayWidget::tr("NO DATA")}},
        {Qn::AccessDeniedOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white,
                QnStatusOverlayWidget::tr("NO ACCESS"),
                kRestrictIconPath}},
        {Qn::NoExportPermissionOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white,
                QnStatusOverlayWidget::tr("NO EXPORT PERMISSION"),
                kRestrictIconPath}},
        {Qn::TooManyOpenedConnectionsOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white,
                QnStatusOverlayWidget::tr("TOO MANY CONNECTIONS")}},
        {Qn::NoLiveStreamOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white,
                QnStatusOverlayWidget::tr("NO LIVE STREAM")}},
        {Qn::CannotDecryptMediaOverlay,
            {QnStatusOverlayWidget::ErrorStyle::white,
                QnStatusOverlayWidget::tr("ARCHIVE ENCRYPTED"),
                kLockIconPath}},
        {Qn::MismatchedCertificateOverlay,
            {QnStatusOverlayWidget::ErrorStyle::red,
                QnStatusOverlayWidget::tr("CERTIFICATE ERROR"),
                kErrorIconPath}},
    };

    return kOverlayInfo;
}

QnStatusOverlayWidget::ErrorStyle overlayErrorStyle(Qn::ResourceStatusOverlay overlay)
{
    if (auto it = overlayInfo().find(overlay); NX_ASSERT(it != overlayInfo().end()))
        return it->second.style;

    return QnStatusOverlayWidget::ErrorStyle::red;
}

QString statusIconPath(Qn::ResourceStatusOverlay overlay)
{
    if (auto it = overlayInfo().find(overlay); NX_ASSERT(it != overlayInfo().end()))
        return it->second.statusIconPath;

    return {};
}

QString tooltip(Qn::ResourceStatusOverlay overlay)
{
    if (auto it = overlayInfo().find(overlay);
        NX_ASSERT(it != overlayInfo().end()) && !it->second.tooltip.isEmpty())
    {
        return it->second.tooltip;
    }

    return {};
}

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
    m_resource(resource),
    m_widget(widget),
    m_buttonTexts(getButtonCaptions(resource)),
    m_visibleItems(QnStatusOverlayWidget::Control::kNoControl),
    m_statusOverlay(Qn::EmptyOverlay),
    m_currentButton(Qn::ResourceOverlayButton::Empty),
    m_isErrorOverlay(isErrorOverlayCheck(m_statusOverlay)),
    m_errorStyle(QnStatusOverlayWidget::ErrorStyle::red)
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
        [this]() { m_widget->setUseErrorStyle(isErrorOverlay()); });
    connect(this, &QnStatusOverlayController::isErrorStyleChanged, this,
        [this]() { m_widget->setErrorStyle(errorStyle()); });

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

    m_widget->setCaption(caption(m_statusOverlay));

    m_widget->setIcon(statusIcon(m_statusOverlay));
    m_widget->setTooltip(tooltip(m_statusOverlay));
    m_widget->setShowGlow(m_statusOverlay != Qn::LoadingOverlay && m_statusOverlay != Qn::EmptyOverlay);

    updateErrorState();
    updateErrorStyle();
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

QnStatusOverlayWidget::ErrorStyle QnStatusOverlayController::errorStyle() const
{
    return m_errorStyle;
}

void QnStatusOverlayController::updateErrorState()
{
    bool isError = isErrorOverlayCheck(statusOverlay());
    if (m_isErrorOverlay == isError)
        return;

    m_isErrorOverlay = isError;
    emit isErrorOverlayChanged();
}

void QnStatusOverlayController::updateErrorStyle()
{
    auto errorStyle = overlayErrorStyle(statusOverlay());
    if (m_errorStyle == errorStyle)
        return;

    m_errorStyle = errorStyle;
    emit isErrorStyleChanged();
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

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions&
    QnStatusOverlayController::statusIconColors(Qn::ResourceStatusOverlay overlay)
{
    if (isErrorOverlayCheck(overlay)
        && overlayErrorStyle(overlay) == QnStatusOverlayWidget::ErrorStyle::white)
    {
        return kWhiteTheme;
    }

    return kRedTheme;
}

QPixmap QnStatusOverlayController::statusIcon(Qn::ResourceStatusOverlay status)
{
    const auto pixmapPath = statusIconPath(status);
    const auto colorTheme = statusIconColors(status);

    if (pixmapPath.isEmpty())
        return QPixmap();

    constexpr QSize kIconSize = {48, 48};
    return colorTheme.isEmpty() ? qnSkin->pixmap(pixmapPath, true, kIconSize)
                                : qnSkin->icon(pixmapPath, colorTheme).pixmap(kIconSize);
}

QString QnStatusOverlayController::caption(Qn::ResourceStatusOverlay overlay)
{
    if (auto it = overlayInfo().find(overlay); NX_ASSERT(it != overlayInfo().end()))
        return it->second.caption;

    return {};
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
    result.insert(toInt(Qn::ResourceOverlayButton::EnableLicense), tr("Activate License"));
    result.insert(toInt(Qn::ResourceOverlayButton::MoreLicenses), tr("Activate License"));
    result.insert(toInt(Qn::ResourceOverlayButton::SetPassword), tr("Setup"));
    result.insert(toInt(Qn::ResourceOverlayButton::UnlockEncryptedArchive), tr("Unlock"));
    result.insert(toInt(Qn::ResourceOverlayButton::RequestInformation), tr("Provide"));
    result.insert(toInt(Qn::ResourceOverlayButton::Authorize), tr("Authorize"));

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
