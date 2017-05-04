#include "recording_stats_item_delegate.h"

#include <QtWidgets/QApplication>

#include <client/client_globals.h>

#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/recording_stats_model.h>
#include <ui/style/helper.h>

#include <utils/common/scoped_painter_rollback.h>

namespace {

const int kMinColumnWidth = 160;
const int kTotalLineOffset = 4;

const int kTotalCamerasFontWeight = QFont::Bold;
const int kForeignCamerasFontWeight = QFont::Normal;
const int kFontWeight = QFont::DemiBold;

} // namespace

QnRecordingStatsItemDelegate::QnRecordingStatsItemDelegate(QObject* parent) :
    base_type(parent),
    m_resourceDelegate(new QnResourceItemDelegate())
{
}

QnRecordingStatsItemDelegate::~QnRecordingStatsItemDelegate()
{
}

const QnRecordingStatsColors& QnRecordingStatsItemDelegate::colors() const
{
    return m_colors;
}

void QnRecordingStatsItemDelegate::setColors(const QnRecordingStatsColors& colors)
{
    m_colors = colors;
}

void QnRecordingStatsItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    NX_ASSERT(index.isValid());
    const bool isCameraName = index.column() == QnRecordingStatsModel::CameraNameColumn;

    const auto type = rowType(index);
    if (isCameraName && (type == RowType::normal))
    {
        m_resourceDelegate->paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();

    QString label = opt.text;

    opt.text = QString();

    const bool isTotal = (type == RowType::total);

    if (isTotal)
        opt.rect = opt.rect.adjusted(0, kTotalLineOffset, 0, 0);

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    if (!isCameraName)
    {
        /* Draw chart manually: */
        qreal chartData = index.data(Qn::RecordingStatChartDataRole).toReal();
        QRect chartRect = opt.rect.adjusted(style::Metrics::kStandardPadding, 1, -1, -1);
        chartRect.setWidth(chartRect.width() * chartData);
        painter->fillRect(chartRect, m_colors.chartBackground);
    }

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    QnScopedPainterPenRollback penRollback(painter, opt.palette.color(QPalette::Text));
    QnScopedPainterFontRollback fontRollback(painter, opt.font);

    if (index.column() == QnRecordingStatsModel::CameraNameColumn)
        label = opt.fontMetrics.elidedText(label, Qt::ElideRight, textRect.width());

    painter->drawText(textRect, opt.displayAlignment | Qt::TextSingleLine, label);
    if (isTotal)
    {
        painter->setPen(opt.palette.color(QPalette::Mid));
        painter->drawLine(opt.rect.topLeft(), opt.rect.topRight());
    }
}

QSize QnRecordingStatsItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto type = rowType(index);
    const bool isNormal = (type == RowType::normal);

    if (isNormal && index.column() == QnRecordingStatsModel::CameraNameColumn)
        return m_resourceDelegate->sizeHint(option, index);

    QSize size = base_type::sizeHint(option, index);
    if (isNormal)
        size.setWidth(std::max(size.width(), kMinColumnWidth));
    else if (type == RowType::total)
        size.setHeight(size.height() + kTotalLineOffset * 3);

    return size;
}

void QnRecordingStatsItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    switch (rowType(index))
    {
        case RowType::normal:
            option->font.setWeight(kFontWeight);
            option->palette.setColor(QPalette::Text, m_colors.chartForeground);
            break;
        case RowType::foreign:
            option->font.setWeight(kForeignCamerasFontWeight);
            option->palette.setColor(QPalette::Text,
                index.column() == QnRecordingStatsModel::CameraNameColumn
                    ? m_colors.foreignForeground
                    : m_colors.chartForeground);
                break;
        case RowType::total:
            option->font.setWeight(kTotalCamerasFontWeight);
            option->palette.setColor(QPalette::Text, m_colors.totalForeground);
            break;
    }

    if (index.column() == QnRecordingStatsModel::CameraNameColumn)
        option->displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    else
        option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;

    option->fontMetrics = QFontMetrics(option->font);
}

QnRecordingStatsItemDelegate::RowType QnRecordingStatsItemDelegate::rowType(const QModelIndex& index) const
{
    auto data = index.data(Qn::RecordingStatsDataRole).value<QnCamRecordingStatsData>();

    if (data.uniqueId.isEmpty())
        return RowType::total;

    if (data.uniqueId == QnSortedRecordingStatsModel::kForeignCameras)
        return RowType::foreign;

    return RowType::normal;
}
