
#include "status_overlay_controller.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <utils/common/scoped_value_rollback.h>
#include <ui/style/skin.h>

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
        case Qn::PausedOverlay:
        case Qn::LoadingOverlay:
        case Qn::NoDataOverlay:
        case Qn::NoVideoDataOverlay:
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
        [this]
        {
            emit buttonClicked(m_currentButton);
        });

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

    const auto pixmapPath = statusIcon(statusOverlay());
    m_widget->setIconOverlayPixmap(pixmapPath.isEmpty()
        ? QPixmap()
        : qnSkin->pixmap(pixmapPath, true));
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

    const auto pixmapPath = statusIcon(m_statusOverlay);
    m_widget->setIcon(pixmapPath.isEmpty() ? QPixmap() : qnSkin->pixmap(pixmapPath, true));

    updateErrorState();
    updateVisibleItems();
}

QnStatusOverlayWidget::Controls QnStatusOverlayController::errorVisibleItems()
{
    const auto overlay = statusOverlay();

    QnStatusOverlayWidget::Controls result = QnStatusOverlayWidget::Control::kCaption;

    if (!statusIcon(overlay).isEmpty())
        result |= QnStatusOverlayWidget::Control::kIcon;

    if (m_currentButton != Qn::ResourceOverlayButton::Empty)
        result |= QnStatusOverlayWidget::Control::kButton;

    if (!descriptionText(overlay).isEmpty())
        result |= QnStatusOverlayWidget::Control::kDescription;

    if (!m_customButtonText.isEmpty())
        result |= QnStatusOverlayWidget::Control::kCustomButton;

    return result;
}

QnStatusOverlayWidget::Controls QnStatusOverlayController::normalVisibleItems()
{
    switch (statusOverlay())
    {
        case Qn::LoadingOverlay:
            return QnStatusOverlayWidget::Control::kPreloader;
        case Qn::PausedOverlay:
        case Qn::NoVideoDataOverlay:
            return QnStatusOverlayWidget::Control::kImageOverlay;
        case Qn::NoDataOverlay:
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
    static const auto kCaptions =
        []() -> IntStringHash
        {
            const auto kNotEnoughLicenses = tr("NOT ENOUGH LICENCES");
            IntStringHash result;
            result[toInt(Qn::NoDataOverlay)] = tr("NO DATA");
            result[toInt(Qn::UnauthorizedOverlay)] = tr("UNAUTHORIZED");
            result[toInt(Qn::OfflineOverlay)] = tr("NO SIGNAL");
            result[toInt(Qn::AnalogWithoutLicenseOverlay)] = kNotEnoughLicenses;
            result[toInt(Qn::VideowallWithoutLicenseOverlay)] = kNotEnoughLicenses;
            result[toInt(Qn::ServerOfflineOverlay)] = tr("SERVER UNAVAILABLE");
            result[toInt(Qn::ServerUnauthorizedOverlay)] = tr("NO ACCESS");
            result[toInt(Qn::IoModuleDisabledOverlay)] = tr("DEVICE DISABLED");
            result[toInt(Qn::PasswordRequiredOverlay)] = tr("PASSWORD REQUIRED");
            return result;
        }();

    return extractValue(overlay, kCaptions);
}

QString QnStatusOverlayController::descriptionText(Qn::ResourceStatusOverlay overlay)
{
    return (overlay == Qn::UnauthorizedOverlay
        ? tr("Please check authentication information")
        : QString());
}

QString QnStatusOverlayController::statusIcon(Qn::ResourceStatusOverlay overlay)
{
    static const auto kIconPaths =
        []() -> IntStringHash
        {
            const auto kLicenceIconPath = lit("item_placeholders/license.png");;
            IntStringHash result;
            result[toInt(Qn::UnauthorizedOverlay)] = lit("item_placeholders/unauthorized.png");
            result[toInt(Qn::OfflineOverlay)] = lit("item_placeholders/no_signal.png");
            result[toInt(Qn::AnalogWithoutLicenseOverlay)] = kLicenceIconPath;
            result[toInt(Qn::VideowallWithoutLicenseOverlay)] = kLicenceIconPath;
            result[toInt(Qn::ServerUnauthorizedOverlay)] = lit("item_placeholders/no_access.png");
            result[toInt(Qn::IoModuleDisabledOverlay)] = lit("item_placeholders/disabled.png");
            result[toInt(Qn::NoVideoDataOverlay)] = lit("legacy/io_speaker.png");
            result[toInt(Qn::PausedOverlay)] = lit("item_placeholders/pause.png");
            result[toInt(Qn::PasswordRequiredOverlay)] = lit("item_placeholders/alert.png");
            return result;
        }();

    return extractValue(overlay, kIconPaths);
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
    result.insert(toInt(Qn::ResourceOverlayButton::IoEnable), tr("Enable"));
    result.insert(toInt(Qn::ResourceOverlayButton::MoreLicenses), tr("Activate License"));
    result.insert(toInt(Qn::ResourceOverlayButton::SetPassword), tr("Set For This Camera"));

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
