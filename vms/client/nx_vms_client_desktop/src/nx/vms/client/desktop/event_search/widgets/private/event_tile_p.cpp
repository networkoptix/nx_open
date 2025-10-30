// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_tile_p.h"
#include "ui_event_tile.h"

#include <QtGui/QTextDocument>

#include <health/system_health_strings_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/common/utils/text_utils.h>
#include <nx/vms/client/core/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/common/widgets/close_button.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/time/formatter.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

using namespace std::chrono;

namespace {

static constexpr qsizetype kResourcesInforemersTextSize = 12;
// TODO: #vbutkevich make value dynamic, after removing QnSystemHealthStringsHelper::resourceText.
static constexpr qsizetype kMaxResourceTextWidth = 50;
static constexpr qsizetype kResourcesTextSize = 10;
static constexpr qsizetype kMaximumResourceInformersLineLimit = 3;
static constexpr qsizetype kMaximumResourceLineLimit = 1;
static constexpr char kMultipleResourcesSeparator = ',';
static constexpr int kTileTitleLineLimit = 2;
static constexpr int kTileDescriptionLineLimit = 5;
static constexpr auto kRelativeTimestampUpdateTime = 30s;

} // namespace

namespace nx::vms::client::desktop {

EventTile::Private::Private(EventTile* q):
    q(q),
    progressLabel(new QnElidedLabel(q)),
    loadPreviewTimer(new QTimer(q)),
    timestampLabelUpdateTimer(new QTimer(q))
{
    loadPreviewTimer->setSingleShot(true);
    QObject::connect(loadPreviewTimer.get(), &QTimer::timeout, [this]() { requestPreview(); });

    timestampLabelUpdateTimer->setInterval(kRelativeTimestampUpdateTime);
    QObject::connect(timestampLabelUpdateTimer.get(), &QTimer::timeout,
        [this]() { updateTimestampLabel(); });
}

void EventTile::Private::initLabelDescriptor(const QVariant& value, LabelDescriptor& descriptor)
{
    descriptor.fullContent = value;
    descriptor.currentWidth = 0;
    descriptor.textByWidth.clear();
}

void EventTile::Private::closeRequested()
{
    if (onCloseAction)
        onCloseAction->trigger();

    emit q->closeRequested();
}

void EventTile::Private::setDescription(const QString& value)
{
    if (descriptionLabelDescriptor.fullContent == value)
        return;

    initLabelDescriptor(value, descriptionLabelDescriptor);
    descriptionLabelDescriptor.formatter =
        [&]()
        {
            return getElidedStringByRowsNumberAndWidth(q->ui->descriptionLabel,
                descriptionLabelDescriptor.fullContent.toString(),
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
            return getElidedStringByRowsNumberAndWidth(q->ui->nameLabel,
            titleLabelDescriptor.fullContent.toString(),
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
            q->closeButton()->setVisible(showCloseButton);
            q->setBackgroundRole(hovered ? QPalette::Midlight : QPalette::Window);

            if (showCloseButton)
                q->closeButton()->raise();
            break;
        }
        case State::pressed:
        {
            q->closeButton()->setVisible(false);
            q->setBackgroundRole(QPalette::Dark);
            break;
        }
    }
}

void EventTile::Private::updatePalette()
{
    auto pal = q->palette();

    if (core::colorTheme()->isInverted())
    {
        const auto base = core::colorTheme()->color("@light1");

        int darkerBy = highlighted ? 1 : 0;
        if (style != Style::informer)
            darkerBy += 3;

        pal.setColor(QPalette::Window, core::colorTheme()->darker(base, darkerBy));
        pal.setColor(QPalette::Midlight, core::colorTheme()->darker(base, darkerBy + 1));
        pal.setColor(QPalette::Dark, core::colorTheme()->color("@light3"));
    }
    else
    {
        const auto base = core::colorTheme()->color("dark5");

        int lighterBy = highlighted ? 1 : 0;
        if (style == Style::informer)
            lighterBy += 2;

        pal.setColor(QPalette::Window, core::colorTheme()->lighter(base, lighterBy));
        pal.setColor(QPalette::Midlight, core::colorTheme()->lighter(base, lighterBy + 1));
        pal.setColor(QPalette::Dark, core::colorTheme()->darker(base, 2 - lighterBy));
    }

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
    q->ui->iconLabel->setPixmap(qnSkin->icon(iconPath, colorTheme).pixmap(20, 20));
}

qreal EventTile::Private::getWidthOfText(const QString& text, const QLabel* label) const
{
    return QFontMetricsF(label->font()).boundingRect(text).width();
}

QString EventTile::Private::getElidedResourceTextForInformers(const QStringList& list)
{
    if (list.empty())
        return {};

    return QnSystemHealthStringsHelper::resourceText(
        list, kMaxResourceTextWidth, kMaximumResourceInformersLineLimit);
}

void EventTile::Private::updateResourceListStyle()
{
    auto font = q->ui->resourceListLabel->font();
    font.setPixelSize(
        style == Style::standard ? kResourcesTextSize : kResourcesInforemersTextSize);
    q->ui->resourceListLabel->setFont(font);
}

QString EventTile::Private::getElidedResourceText(const QStringList& list)
{
    if (q->visualStyle() == Style::informer)
        return getElidedResourceTextForInformers(list);

    if (list.empty())
        return tr("UNKNOWN");

    const auto maxWidth = q->ui->resourceListLabel->width();
    if (list.size() == 1)
    {
        return getElidedStringByRowsNumberAndWidth(q->ui->resourceListLabel,
            list.front(),
            kMaximumResourceLineLimit);
    }

    // If there are multiple sources,
    // and all of them can fit into one line, we display all of them using separator.
    QString joinedResources = list.join(kMultipleResourcesSeparator);
    if (getWidthOfText(joinedResources, q->ui->resourceListLabel) < maxWidth)
        return joinedResources;

    // If we need to show '<Source 1> + <N>', and it doesn't fit into one line, we
    // truncate Source 1 and add an ellipsis ... after the truncated name.
    // <N> is (total number of sources - 1).
    QString additionalText = QString(" + %1").arg(list.size() - 1);
    if (getWidthOfText(list.front() + additionalText, q->ui->resourceListLabel) > maxWidth)
    {
        return getElidedStringByRowsNumberAndWidth(q->ui->resourceListLabel,
            list.front(),
            kMaximumResourceLineLimit);
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
    return q->imageProvider() && q->previewEnabled() && !obfuscationEnabled;
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
        else if (q->ui->imagePreviewWidget->imageProvider()
            || !q->ui->imagePreviewWidget->placeholder().isEmpty()
            && !obfuscationEnabled)
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

QString EventTile::Private::timestampText() const
{
    const auto systemContext = appContext()->currentSystemContext();
    if (timestampMs <= 0ms || !NX_ASSERT(systemContext))
        return {};

    if (displayRelativeTimestamp())
    {
        const auto secondsAgo = duration_cast<seconds>(milliseconds(
            qnSyncTime->currentMSecsSinceEpoch() - timestampMs.count()));
        return nx::vms::time::fromNow(secondsAgo).toUpper();
    }

    const auto dateTime = systemContext->serverTimeWatcher()->displayTime(timestampMs.count());
    return nx::vms::time::toString(dateTime, nx::vms::time::Format::dd_MM_yy_hh_mm_ss);
}

bool EventTile::Private::displayRelativeTimestamp() const
{
    if (timestampMs <= 0ms)
        return false;

    const auto timestampDeltaMs = milliseconds(
        qnSyncTime->currentMSecsSinceEpoch() - timestampMs.count());

    if (timestampDeltaMs >= 24h)
        return false;

    return true;
}

void EventTile::Private::updateTimestampLabel()
{
    if (displayRelativeTimestamp() != timestampLabelUpdateTimer->isActive())
    {
        if (timestampLabelUpdateTimer->isActive())
            timestampLabelUpdateTimer->stop();
        else
            timestampLabelUpdateTimer->start();
    }

    q->ui->timestampLabel->setText(timestampText());
    q->ui->timestampLabel->setHidden(q->ui->timestampLabel->text().isEmpty());
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
    QLabel* containerLabel, const QString& text, int rowLimit)
{
    if (!containerLabel)
        return {};

    QTextDocument doc;
    doc.setDefaultFont(containerLabel->font());
    doc.setDocumentMargin(containerLabel->margin());
    doc.setHtml(common::html::toHtml(text));
    doc.setTextWidth(containerLabel->width());
    core::text_utils::elideDocumentLines(&doc, rowLimit, true, "...");
    // Must return html, since many strings in labels are created with html formatting.
    return doc.toHtml();
}

} // namespace nx::vms::client::desktop
