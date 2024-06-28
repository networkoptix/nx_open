// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_tile_p.h"
#include "ui_event_tile.h"

#include <QtGui/QTextDocument>

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/common/widgets/close_button.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/delayed.h>

namespace {
static constexpr qsizetype kMaximumResourceLineLimit = 1;
static constexpr char kMultipleResourcesSeparator = ',';
static constexpr int kTileTitleLineLimit = 2;
static constexpr int kTileDescriptionLineLimit = 4;
} // namespace

using namespace std::chrono;
namespace nx::vms::client::desktop {

EventTile::Private::Private(EventTile* q):
    q(q),
    closeButton(new CloseButton(q)),
    closeButtonAnchor(anchorWidgetToParent(closeButton, Qt::RightEdge | Qt::TopEdge)),
    progressLabel(new QnElidedLabel(q)),
    loadPreviewTimer(new QTimer(q))
{
    loadPreviewTimer->setSingleShot(true);
    QObject::connect(loadPreviewTimer.get(), &QTimer::timeout, [this]() { requestPreview(); });
}

void EventTile::Private::initLabelDescriptor(const QVariant& value, LabelDescriptor& descriptor)
{
    descriptor.fullContent = value;
    descriptor.currentWidth = 0;
    descriptor.textByWidth.clear();
}

void EventTile::Private::setDescription(const QString& value)
{
    if (descriptionLabelDescriptor.fullContent == value)
        return;

    initLabelDescriptor(value, descriptionLabelDescriptor);
    descriptionLabelDescriptor.formatter =
        [&]()
        {
            return getElidedStringByRowsNumberAndWidth(q->ui->descriptionLabel->font(),
                descriptionLabelDescriptor.fullContent.toString(),
                q->ui->descriptionLabel->width(),
                kTileDescriptionLineLimit);
        };
    updateLabelForCurrentWidth(q->ui->descriptionLabel, descriptionLabelDescriptor);
}

void EventTile::Private::clearLabel(QLabel* label, LabelDescriptor& descriptor)
{
    descriptor.fullContent.clear();
    descriptor.textByWidth.clear();
    descriptor.currentWidth = 0;
    label->setText({});
}

void EventTile::Private::setTitle(const QString& value)
{
    if (titleLabelDescriptor.fullContent == value)
        return;

    initLabelDescriptor(value, titleLabelDescriptor);
    titleLabelDescriptor.formatter =
        [&]()
        {
            return getElidedStringByRowsNumberAndWidth(q->ui->nameLabel->font(),
                titleLabelDescriptor.fullContent.toString(),
                q->ui->nameLabel->width(),
                kTileTitleLineLimit);
        };
    updateLabelForCurrentWidth(q->ui->nameLabel, titleLabelDescriptor);
}

void EventTile::Private::updateLabelForCurrentWidth(QLabel* label, LabelDescriptor& descriptor)
{
    const auto width = label->width();
    // For some reason, have to explicitly check for width == 1,
    // since without it width changes constantly in loop.
    if (width == 0 || width == 1 || width == descriptor.currentWidth)
        return;

    QString text;
    if (const auto textPtr = descriptor.textByWidth.object(width))
    {
        text = *textPtr;
    }
    else
    {
        text = descriptor.formatter ? descriptor.formatter() : text;
        descriptor.textByWidth.insert(width, new QString(text));
    }
    if (label->text() != text)
        label->setText(text);

    if (!text.isEmpty())
        label->show();

    descriptor.currentWidth = width;
}

void EventTile::Private::handleStateChanged(State state)
{
    if (qApp->activePopupWidget() || qApp->activeModalWidget())
        return;

    switch (state)
    {
        case State::hoverOn:
        case State::hoverOff:
        {
            const bool hovered = state == State::hoverOn;
            const auto showCloseButton = (hovered || q->progressBarVisible()) && closeable;
            closeButton->setVisible(showCloseButton);
            q->setBackgroundRole(hovered ? QPalette::Midlight : QPalette::Window);

            if (showCloseButton)
                closeButton->raise();
            break;
        }
        case State::pressed:
        {
            closeButton->setVisible(false);
            q->setBackgroundRole(QPalette::Dark);
            break;
        }
    }
}

void EventTile::Private::updatePalette()
{
    auto pal = q->palette();
    const auto base = pal.color(QPalette::Base);

    int lighterBy = highlighted ? 1 : 0;
    if (style == Style::informer)
        ++lighterBy;

    pal.setColor(QPalette::Window, core::colorTheme()->lighter(base, lighterBy));
    pal.setColor(QPalette::Midlight, core::colorTheme()->lighter(base, lighterBy + 1));
    pal.setColor(QPalette::Dark, core::colorTheme()->darker(base, 2 - lighterBy));
    q->setPalette(pal);
}

void EventTile::Private::updateIcon()
{
    // Icon label is always visible. It keeps column width fixed.

    if (iconPath.isEmpty())
    {
        q->ui->iconLabel->setPixmap({});
        return;
    }

    core::SvgIconColorer::ThemeSubstitutions colorTheme =
        {{QnIcon::Normal, {.primary = q->titleColor().toRgb().name().toLatin1()}}};
    q->ui->iconLabel->setPixmap(qnSkin->icon(iconPath, colorTheme).pixmap(16, 16));
}

qreal EventTile::Private::getWidthOfText(const QString& text, const QLabel* label) const
{
    return QFontMetricsF(label->font()).boundingRect(text).width();
}

QString EventTile::Private::getElidedResourceText(const QStringList& list)
{
    if (list.empty())
        return q->visualStyle() == Style::informer ? "" : tr("UNKNOWN");

    const auto maxWidth = q->ui->resourceListLabel->width();
    if (list.size() == 1)
    {
        return getElidedStringByRowsNumberAndWidth(
            q->ui->resourceListLabel->font(), list.front(), maxWidth, kMaximumResourceLineLimit);
    }

    // If there are multiple sources,
    // and all of them can fit into one line, we display all of them using separator.
    QString joinedResources = list.join(kMultipleResourcesSeparator);
    if (getWidthOfText(joinedResources, q->ui->resourceListLabel) < maxWidth)
        return joinedResources;

    // If we need to show '<Source 1> + <N>', and it doesn't fit into one line, we
    // truncate Source 1 and add an ellipsis (...) after the truncated name.
    // <N> is (total number of sources - 1).
    QString additionalText = tr(" + %n", "", list.size() - 1);
    if (getWidthOfText(list.front() + additionalText, q->ui->resourceListLabel) > maxWidth)
    {
        return getElidedStringByRowsNumberAndWidth(
            q->ui->resourceListLabel->font(), list.front(), maxWidth, kMaximumResourceLineLimit);
    }

    // If there are multiple sources, and all of them CAN NOT fit into one line,
    // we display the first source fully and add ' + <N>' afterwards.
    return list.front() + additionalText;
}

void EventTile::Private::setResourceList(const QStringList& list)
{
    if (resourceLabelDescriptor.fullContent == list)
        return;

    initLabelDescriptor(QVariant::fromValue(list), resourceLabelDescriptor);
    resourceLabelDescriptor.formatter =
        [&]()
        {
            return getElidedResourceText(resourceLabelDescriptor.fullContent.value<QStringList>());
        };

    updateLabelForCurrentWidth(q->ui->resourceListLabel, resourceLabelDescriptor);

    q->ui->resourceListHolder->setHidden(q->ui->resourceListLabel->text().isEmpty());
}

bool EventTile::Private::isPreviewNeeded() const
{
    return q->imageProvider() && q->previewEnabled();
}

bool EventTile::Private::isPreviewUpdateRequired() const
{
    if (!isPreviewNeeded() || !NX_ASSERT(q->imageProvider()))
        return false;

    if (forceNextPreviewUpdate)
        return true;

    switch (q->imageProvider()->status())
    {
        case core::ThumbnailStatus::Invalid:
        case core::ThumbnailStatus::NoData:
            return true;

        default:
            return false;
    }
}

void EventTile::Private::requestPreview()
{
    if (!isPreviewUpdateRequired())
        return;

    NX_VERBOSE(this, "Requesting tile preview");
    forceNextPreviewUpdate = false;
    isPreviewLoadNeeded = true;
    emit q->needsPreviewLoad();
}

void EventTile::Private::updatePreview(std::chrono::milliseconds delay)
{
    if (delay <= 0ms)
    {
        // Still must be delayed.
        if (isPreviewUpdateRequired())
            executeLater([this]() { requestPreview(); }, q);
    }
    else
    {
        if (isPreviewUpdateRequired())
            loadPreviewTimer->start(delay);
        else
            loadPreviewTimer->stop();
    }
}

void EventTile::Private::showDebugPreviewTimestamp()
{
    auto provider = qobject_cast<core::ResourceThumbnailProvider*>(q->imageProvider());
    if (provider)
    {
        q->ui->debugPreviewTimeLabel->setText(
            nx::format("Preview: %2 us").arg(provider->timestamp().count()));
        q->ui->debugPreviewTimeLabel->setVisible(
            provider->status() == core::ThumbnailStatus::Loaded);
    }
    else
    {
        q->ui->debugPreviewTimeLabel->hide();
        q->ui->debugPreviewTimeLabel->setText({});
    }
}

void EventTile::Private::updatePreviewsVisibility()
{
    if (previewEnabled)
    {
        if (q->ui->videoPreviewWidget->camera())
        {
            q->ui->videoPreviewWidget->show();
            q->ui->imagePreviewWidget->hide();
            q->ui->imagePreviewWidget->parentWidget()->show();
            return;
        }
        else if (q->ui->imagePreviewWidget->imageProvider())
        {
            q->ui->videoPreviewWidget->hide();
            q->ui->imagePreviewWidget->show();
            q->ui->imagePreviewWidget->parentWidget()->show();
            return;
        }
    }

    q->ui->imagePreviewWidget->hide();
    q->ui->videoPreviewWidget->hide();
    q->ui->imagePreviewWidget->parentWidget()->hide();
}

void EventTile::Private::setWidgetHolder(QWidget* widget, QWidget* newHolder)
{
    auto oldHolder = widget->parentWidget();
    const bool wasHidden = widget->isHidden();
    oldHolder->layout()->removeWidget(widget);
    widget->setParent(newHolder);
    newHolder->layout()->addWidget(widget);
    widget->setHidden(wasHidden);
}

QString EventTile::Private::getSystemName(const QString& systemId)
{
    if (systemId.isEmpty())
        return QString();

    auto systemDescription = appContext()->systemFinder()->getSystem(systemId);
    if (!NX_ASSERT(systemDescription))
        return QString();

    return systemDescription->name();
}

// For cloud notifications the system name must be displayed. The function returns the name of
// a system different from the current one in the form required for display.
QString EventTile::Private::getFormattedSystemNameIfNeeded(const QString& systemId)
{
    if (auto systemName = getSystemName(systemId); !systemName.isEmpty())
        return systemName + " / ";

    return QString();
}

QString EventTile::Private::getElidedStringByRowsNumberAndWidth(
    const QFont& font, const QString& text, int textWidth, int rowLimit)
{
    QTextDocument doc;
    doc.setDefaultFont(font);
    doc.setHtml(common::html::toHtml(text));
    doc.setTextWidth(textWidth);
    WidgetUtils::elideDocumentLines(&doc, rowLimit, true, "(...)");
    return doc.toHtml();
}

} // namespace nx::vms::client::desktop
