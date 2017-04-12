#include "audit_item_delegate.h"

#include <QtGui/QMouseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractScrollArea>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <ui/models/audit/audit_log_model.h>
#include <ui/style/helper.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

namespace
{
    /* Default text flags: */
    const int kTextFlags = Qt::TextSingleLine | Qt::AlignLeft | Qt::AlignVCenter;

    /* Table row height: */
    const int kRowHeight = style::Metrics::kViewRowHeight;

    /* Left margin before circular marker indicating archive presence/absence: */
    const int kArchiveMarkerMargin = 2;

    /* Margin at the top, bottom and to the right from description column button: */
    const int kItemButtonMargin = 2;

    /* Margin at the top and bottom of each detail line in description: */
    const int kDetailLineVerticalMargin = 1;

    /* Measure text width the right way: */
    int textWidth(const QFont& font, const QString& text)
    {
        return QFontMetrics(font).size(Qt::TextSingleLine, text).width();
    }

    /* Get description button text from audit record: */
    QString itemButtonText(const QnAuditRecord* record)
    {
        if (!record)
            return QString();

        switch (record->eventType)
        {
            case Qn::AR_ViewLive:
            case Qn::AR_ViewArchive:
            case Qn::AR_ExportVideo:
                return QnAuditItemDelegate::tr("Play");

            case Qn::AR_UserUpdate:
                return QnAuditItemDelegate::tr("User settings...");

            case Qn::AR_ServerUpdate:
                return QnAuditItemDelegate::tr("Server settings...");

            case Qn::AR_CameraUpdate:
            case Qn::AR_CameraInsert:
                return QnAuditItemDelegate::tr("Camera settings...");

            default:
                return QString();
        }
    }

    /* Calculate description button rectangle by its text: */
    QRect itemButtonRect(const QStyleOptionViewItem& option, const QString& buttonText)
    {
        if (buttonText.isEmpty())
            return QRect();

        QSize buttonSize = QSize(option.fontMetrics.size(Qt::TextSingleLine, buttonText).width()
                               + style::Metrics::kStandardPadding * 2,
            kRowHeight - kItemButtonMargin * 2);

        QRect rect(option.rect);
        rect.setRight(rect.right() - kItemButtonMargin);
        rect.setHeight(kRowHeight);

        return QStyle::alignedRect(Qt::LeftToRight, Qt::AlignRight | Qt::AlignVCenter, buttonSize, rect);
    }

    /* Calculate description button rectangle by audit record: */
    QRect itemButtonRect(const QStyleOptionViewItem& option, const QnAuditRecord* record)
    {
        return itemButtonRect(option, itemButtonText(record));
    }

    /* Calculate description button rectangle by model index: */
    QRect itemButtonRect(const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        if (index.data(Qn::ColumnDataRole).toInt() != QnAuditLogModel::DescriptionColumn)
            return QRect();

        return itemButtonRect(option, index.data(Qn::AuditRecordDataRole).value<QnAuditRecord*>());
    }
};

QnAuditItemDelegate::QnAuditItemDelegate(QObject* parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_mouseCapture(kNotCaptured)
{
}

/* Cached text width: */
int QnAuditItemDelegate::cachedTextWidth(const QFont& font, const QString& textData, bool isBold) const
{
    int width = 0;
    auto& hash = (isBold || font.bold()) ? m_boldSizeHintHash : m_sizeHintHash;

    auto itr = hash.find(textData);
    if (itr != hash.end())
    {
        width = itr.value();
    }
    else
    {
        QFont localFont(font);
        if (isBold)
            localFont.setBold(true);

        width = textWidth(localFont, textData);
        hash.insert(textData, width);
    }

    return width;
}

/* Default computation of optimal item size: */
QSize QnAuditItemDelegate::defaultSizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    return QSize(cachedTextWidth(option.font, index.data().toString(), false), kRowHeight);
}

/* Overridden function to compute optimal item size: */
QSize QnAuditItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    static const QDateTime dateTimePattern = QDateTime::fromString(lit("2015-12-12T23:59:59"), Qt::ISODate);
    static const QString dateTimeStr = dateTimePattern.toString(Qt::DefaultLocaleShortDate);
    static const QString timeStr = dateTimePattern.time().toString(Qt::DefaultLocaleShortDate);
    static const QString dateStr = dateTimePattern.date().toString(Qt::DefaultLocaleShortDate);
    static const QString dateStrWithSpace = dateStr + lit(" ");

    QSize result(0, kRowHeight);
    QnAuditLogModel::Column column = static_cast<QnAuditLogModel::Column>(index.data(Qn::ColumnDataRole).toInt());

    switch (column)
    {
        case QnAuditLogModel::DescriptionColumn:
            result = descriptionSizeHint(option, index);
            break;

        case QnAuditLogModel::TimestampColumn:
            result.setWidth(cachedTextWidth(option.font, dateStrWithSpace)
                          + cachedTextWidth(option.font, timeStr, true));
            break;

        case QnAuditLogModel::EndTimestampColumn:
            result.setWidth(qMax(cachedTextWidth(option.font, dateStrWithSpace)
                               + cachedTextWidth(option.font, timeStr, true),
                cachedTextWidth(option.font, QnAuditLogModel::eventTypeToString(
                    Qn::AR_UnauthorizedLogin))));
            break;

        case QnAuditLogModel::TimeColumn:
            result.setWidth(cachedTextWidth(option.font, timeStr));
            break;

        case QnAuditLogModel::DateColumn:
            result.setWidth(cachedTextWidth(option.font, dateStr, true));
            break;

        case QnAuditLogModel::UserActivityColumn:
            result = index.data(Qt::SizeHintRole).toSize();
            if (!result.isValid())
                result = defaultSizeHint(option, index);
            break;

        default:
            result = defaultSizeHint(option, index);
            break;
    }

    result.setWidth(result.width() + style::Metrics::kStandardPadding * 2);
    return result;
}

/* Computation of description item size: */
QSize QnAuditItemDelegate::descriptionSizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize result = defaultSizeHint(option, index);

    /* Calculate height addition if this record has details and they're open: */
    QnAuditRecord* record = index.data(Qn::AuditRecordDataRole).value<QnAuditRecord*>();
    if (record)
    {
        switch (record->eventType)
        {
            case Qn::AR_ViewLive:
            case Qn::AR_ViewArchive:
            case Qn::AR_ExportVideo:
            case Qn::AR_CameraUpdate:
            case Qn::AR_CameraInsert:
                if (QnAuditLogModel::hasDetail(record))
                {
                    QFont smallFont(option.font);
                    smallFont.setPixelSize(option.font.pixelSize() - 1);
                    QFontMetrics smallFontMetrics(smallFont);
                    result.setHeight(result.height() + (smallFontMetrics.height() + kDetailLineVerticalMargin * 2) * QnAuditLogModel::getCameras(record).size());
                }
                break;

            default:
                break;
        }
    }

    return result;
}

/* Despite its name handles mouse events even when editor is not created: */
bool QnAuditItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (QAbstractScrollArea* scrollArea = qobject_cast<QAbstractScrollArea*>(const_cast<QWidget*>(option.widget)))
    {
        switch (event->type())
        {
            case QEvent::MouseMove:
            {
                /* First, check if mouse was released outside of our widget and discard any captures: */
                if (m_mouseCapture != kNotCaptured && !static_cast<const QMouseEvent*>(event)->buttons().testFlag(Qt::LeftButton))
                {
                    m_lastPressedIndex = QModelIndex();
                    m_mouseCapture = kNotCaptured;
                }

                /* Check if mouse is hovering over item button in description column: */
                QRect buttonRect = itemButtonRect(option, index);
                if (buttonRect.contains(static_cast<const QMouseEvent*>(event)->pos()))
                {
                    /* Hover a button: */
                    if (m_lastHoveredButtonIndex != index)
                    {
                        m_lastHoveredButtonIndex = index;
                        m_lastHoveredButtonRect = buttonRect;
                        scrollArea->viewport()->update(buttonRect);
                    }
                }
                else
                {
                    /* Unhover a button: */
                    if (m_lastHoveredButtonIndex.isValid())
                    {
                        m_lastHoveredButtonIndex = QModelIndex();
                        scrollArea->viewport()->update(m_lastHoveredButtonRect);
                    }
                }

                /* If mouse is captured, skip default handling to avoid drag-selecting: */
                if (m_mouseCapture != kNotCaptured)
                    return true;

                break;
            }

            case QEvent::MouseButtonPress:
            {
                if (static_cast<const QMouseEvent*>(event)->button() == Qt::LeftButton)
                {
                    if (index.data(Qn::ColumnDataRole).toInt() == QnAuditLogModel::DescriptionColumn)
                    {
                        if (m_lastHoveredButtonIndex == index)
                        {
                            /* Item button was pressed: */
                            m_mouseCapture = kCapturedByButton;
                            scrollArea->viewport()->update(m_lastHoveredButtonRect);
                        }
                        else
                        {
                            /* Do not produce clicks on description items without details: */
                            const QnAuditRecord* record = index.data(Qn::AuditRecordDataRole).value<QnAuditRecord*>();
                            if (!record || record->resources.empty())
                                break;

                            /* Description was pressed outside its button: */
                            m_mouseCapture = kCapturedByDescription;
                        }

                        m_lastPressedIndex = index;

                        /* Skip default handling to avoid selection change: */
                        return true;
                    }
                }

                break;
            }

            case QEvent::MouseButtonRelease:
            {
                if (static_cast<const QMouseEvent*>(event)->button() == Qt::LeftButton && m_mouseCapture != kNotCaptured)
                {
                    if (m_lastPressedIndex == index)
                    {
                        switch (m_mouseCapture)
                        {
                        case kNotCaptured:
                            break;

                        case kCapturedByDescription:
                            /* Click on description outside its button: */
                            emit descriptionClicked(index);
                            break;

                        case kCapturedByButton:
                            /* Click on description item button: */
                            scrollArea->viewport()->update(m_lastHoveredButtonRect);
                            emit buttonClicked(index);
                            break;
                        }
                    }

                    m_lastPressedIndex = QModelIndex();
                    m_mouseCapture = kNotCaptured;

                    /* Skip default handling to avoid selection change: */
                    return true;
                }
                break;
            }

            default:
                break;
        }
    }

    return base_type::editorEvent(event, model, option, index);
}

/* Item drawing: */
void QnAuditItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);
    option.features &= ~QStyleOptionViewItem::HasDecoration;

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    QnScopedPainterFontRollback fontRollback(painter, option.font);
    QnScopedPainterPenRollback penRollback(painter, option.palette.color(
        option.state.testFlag(QStyle::State_Selected)
            ? QPalette::HighlightedText
            : QPalette::Text));

    /* Paint alternate background: */
    if (index.data(Qn::AlternateColorRole).toInt() > 0)
    {
        option.features |= QStyleOptionViewItem::Alternate;
        style->drawPrimitive(QStyle::PE_PanelItemViewRow, &option, painter, option.widget);
    }

    /* If item is multi-line: */
    if (option.rect.height() > kRowHeight)
    {
        /* Paint extra space hover and/or selection: */
        option.rect.setTop(option.rect.top() + kRowHeight);
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

        /* Set option rectangle for one line: */
        option.rect.setTop(styleOption.rect.top());
        option.rect.setHeight(kRowHeight);
    }

    /* Draw audit item: */
    const QnAuditRecord* record = index.data(Qn::AuditRecordDataRole).value<QnAuditRecord*>();
    if (record)
    {
        QnAuditLogModel::Column column = static_cast<QnAuditLogModel::Column>(index.data(Qn::ColumnDataRole).toInt());
        switch (column)
        {
            case QnAuditLogModel::TimestampColumn:
                paintDateTime(style, painter, option, record->createdTimeSec);
                paintFocusRect(style, painter, option);
                return;

            case QnAuditLogModel::EndTimestampColumn:
                if (record->eventType == Qn::AR_UnauthorizedLogin || record->rangeEndSec == 0)
                    break;
                paintDateTime(style, painter, option, record->rangeEndSec);
                paintFocusRect(style, painter, option);
                return;

            case QnAuditLogModel::DescriptionColumn:
                paintDescription(style, painter, option, index, record);
                paintFocusRect(style, painter, option);
                return;

            case QnAuditLogModel::UserActivityColumn:
                paintUserActivity(style, painter, option, index);
                paintFocusRect(style, painter, option);
                return;

            default:
                break;
        }
    }

    /* Else perform default painting: */
    base_type::paint(painter, option, index);
}

/* Mixed character style date & time drawing: */
void QnAuditItemDelegate::paintDateTime(const QStyle* style, QPainter* painter, const QStyleOptionViewItem& option, int dateTimeSecs) const
{
    /* Paint item hover and/or selection: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    if (dateTimeSecs == 0)
        return;

    /* Get date and time strings: */
    QDateTime dateTime = context()->instance<QnWorkbenchServerTimeWatcher>()->displayTime(dateTimeSecs * 1000ll);
    QString dateStr = dateTime.date().toString(Qt::DefaultLocaleShortDate) + lit(" ");
    QString timeStr = dateTime.time().toString(Qt::DefaultLocaleShortDate);

    /* Calculate time offset: */
    int timeXOffset = textWidth(option.font, dateStr);

    /* Draw date: */
    QRect rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, dateStr);

    /* Draw time: */
    QFont font = option.font;
    font.setBold(true);
    rect.adjust(timeXOffset, 0, 0, 0);
    QnScopedPainterFontRollback fontRollback(painter, font);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, timeStr);
}

/* Description column complex drawing: */
void QnAuditItemDelegate::paintDescription(const QStyle* style, QPainter* painter,
    const QStyleOptionViewItem& option, const QModelIndex& index, const QnAuditRecord* record) const
{
    /* Paint first line hover and/or selection: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    QnScopedPainterFontRollback fontRollback(painter);
    QnScopedPainterPenRollback penRollback(painter);

    /* Analyze event type and determine what to display: */
    QnVirtualCameraResourceList cameras;
    QString mainText;

    switch (record->eventType)
    {
        case Qn::AR_ViewLive:
        case Qn::AR_ViewArchive:
        case Qn::AR_ExportVideo:
            mainText = lit("%1 - %2").arg(
                context()->instance<QnWorkbenchServerTimeWatcher>()->
                    displayTime(record->rangeStartSec * 1000ll).toString(Qt::DefaultLocaleShortDate)).arg(
                context()->instance<QnWorkbenchServerTimeWatcher>()->
                    displayTime(record->rangeEndSec * 1000ll).toString(Qt::DefaultLocaleShortDate));
            cameras = QnAuditLogModel::getCameras(record);
            break;

        case Qn::AR_CameraUpdate:
        case Qn::AR_CameraInsert:
            cameras = QnAuditLogModel::getCameras(record);
            break;

        default:
            mainText = option.index.data(Qt::DisplayRole).toString();
            break;
    }

    QString linkText;
    if (!cameras.empty())
    {
        linkText = QnDeviceDependentStrings::getNumericName(resourcePool(), cameras);
        if (!mainText.isEmpty())
            mainText += lit(",  ");
    }

    /* Obtain item rectangle: */
    QStyleOptionViewItem viewOption(option);
    viewOption.features = QStyleOptionViewItem::HasDisplay;
    viewOption.text = lit("Some description text");
    QRect rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

    /* Draw item button: */
    QString buttonText = itemButtonText(record);
    if (!buttonText.isEmpty())
    {
        /* Fill style option: */
        QStyleOptionButton button;
        button.fontMetrics = option.fontMetrics;
        button.text = buttonText;
        button.rect = itemButtonRect(option, buttonText);
        button.state = option.state & (QStyle::State_Enabled | QStyle::State_Active);

        if (option.state.testFlag(QStyle::State_MouseOver) && index == m_lastHoveredButtonIndex)
        {
            button.state |= (index == m_lastPressedIndex && m_mouseCapture == kCapturedByButton) ?
                QStyle::State_Sunken :
                QStyle::State_MouseOver;
        }

        /* Draw: */
        QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter, option.widget);

        /* Subtract button and padding from the item rectangle: */
        rect.setRight(button.rect.left() - style::Metrics::kStandardPadding);
    }

    /* Draw main text: */
    QRect actualRect;
    QString elidedMainText = option.fontMetrics.elidedText(mainText, option.textElideMode, rect.width());

    painter->drawText(rect, kTextFlags, elidedMainText, &actualRect);

    /* Draw link text: */
    if (!linkText.isEmpty() && elidedMainText == mainText)
    {
        QRect linkRect(rect);
        if (actualRect.isValid())
            linkRect.setLeft(actualRect.right());

        QString elidedLinkText = option.fontMetrics.elidedText(linkText, option.textElideMode, linkRect.width());
        QColor linkColor = style::linkColor(option.palette, option.state.testFlag(QStyle::State_MouseOver));

        QFont font(option.font);
        painter->setFont(font);
        painter->setPen(linkColor);
        painter->drawText(linkRect, kTextFlags, elidedLinkText);
    }

    /* Draw details (camera list): */
    if (!cameras.empty() && QnAuditLogModel::hasDetail(record))
    {
        QFont smallFont(option.font);
        smallFont.setPixelSize(option.font.pixelSize() - 1);
        painter->setFont(smallFont);
        painter->setPen(option.palette.color(QPalette::WindowText));

        QFontMetrics smallFontMetrics(smallFont);
        int markerRadius = smallFontMetrics.strikeOutPos();
        int nameMargin = markerRadius * 2 + style::Metrics::kStandardPadding + 2;
        QRect nameRect(rect.left() + nameMargin, rect.bottom() + kDetailLineVerticalMargin, option.rect.width() - nameMargin, smallFontMetrics.height());
        QPoint markerCenter(rect.left() + markerRadius + kArchiveMarkerMargin, nameRect.bottom() - smallFontMetrics.descent() - markerRadius);

        auto archiveData = record->extractParam("archiveExist");
        int smallRowHeight = nameRect.height() + kDetailLineVerticalMargin * 2;
        int index = 0;

        for (const auto& camera : cameras)
        {
            /* Draw archive marker: */
            if (record->isPlaybackType())
            {
                bool recordExists = (size_t)index < archiveData.size() && archiveData[index] == '1';
                QnScopedPainterBrushRollback brushRollback(painter, recordExists ? painter->pen().color() : QColor(Qt::transparent));
                painter->drawEllipse(markerCenter, markerRadius, markerRadius);
                index++;
            }

            /* Draw camera name: */
            QString name = QnResourceDisplayInfo(camera).toString(qnSettings->extraInfoInTree());
            QString elidedName = smallFontMetrics.elidedText(name, option.textElideMode, nameRect.width());
            painter->drawText(nameRect, kTextFlags, elidedName);
            nameRect.moveTop(nameRect.top() + smallRowHeight);
            markerCenter.setY(markerCenter.y() + smallRowHeight);
        }
    }
}

/* User activity bar chart & label drawing: */
void QnAuditItemDelegate::paintUserActivity(const QStyle* style, QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    /* Paint item hover and/or selection: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Obtain values: */
    qreal chartData = index.data(Qn::AuditLogChartDataRole).toReal();
    QColor chartColor = index.data(Qt::ForegroundRole).value<QColor>();

    /* Draw chart bar: */
    QRect barRect = option.rect.adjusted(2, 1, -2, -1);
    barRect.setWidth(barRect.width() * chartData);
    painter->fillRect(barRect, chartColor);

    /* If hovered, draw text label: */
    if (option.state.testFlag(QStyle::State_MouseOver))
    {
        QnScopedPainterFontRollback fontRollback(painter, option.font);
        QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::ButtonText));
        QRect labelRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
        painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, index.data().toString());
    }
}

/* Focus rectangle: */
void QnAuditItemDelegate::paintFocusRect(const QStyle* style, QPainter* painter, const QStyleOptionViewItem& option) const
{
    if (!option.state.testFlag(QStyle::State_HasFocus))
        return;

    QStyleOptionFocusRect frameOption;
    frameOption.QStyleOption::operator=(option);
    frameOption.rect = style->subElementRect(QStyle::SE_ItemViewItemFocusRect, &option, option.widget);
    style->drawPrimitive(QStyle::PE_FrameFocusRect, &frameOption, painter, option.widget);
}
