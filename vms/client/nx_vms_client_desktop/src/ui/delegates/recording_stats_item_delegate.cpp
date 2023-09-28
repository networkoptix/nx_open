// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_stats_item_delegate.h"

#include <QtWidgets/QApplication>

#include <client/client_globals.h>

#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/recording_stats_model.h>
#include <nx/vms/client/desktop/style/helper.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

#include <nx/vms/client/core/skin/color_theme.h>

using RowType = QnRecordingStatsModel::RowType;

namespace {

const int kMinColumnWidth = 160;
const int kTotalLineOffset = 4;

const auto kTotalCamerasFontWeight = QFont::Bold;
const auto kForeignCamerasFontWeight = QFont::Normal;
const auto kFontWeight = QFont::DemiBold;

QnRecordingStatsModel::RowType rowType(const QModelIndex& index)
{
    return index.data(QnRecordingStatsModel::RowKind).value<RowType>();
}

} // namespace

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

QnRecordingStatsItemDelegate::QnRecordingStatsItemDelegate(QObject* parent) :
    base_type(parent),
    m_resourceDelegate(new QnResourceItemDelegate())
{
}

QnRecordingStatsItemDelegate::~QnRecordingStatsItemDelegate()
{
}

void QnRecordingStatsItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    NX_ASSERT(index.isValid());
    const bool isCameraName = index.column() == QnRecordingStatsModel::CameraNameColumn;

    const auto type = rowType(index);
    if (isCameraName && (type == RowType::Normal))
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

    const bool isTotal = (type == RowType::Totals);

    if (isTotal)
        opt.rect = opt.rect.adjusted(0, kTotalLineOffset, 0, 0);

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    if (!isCameraName)
    {
        static const QColor kChartBackgroundColor = withAlpha(
            core::colorTheme()->color("brand_core"), 102);

        /* Draw chart manually: */
        qreal chartData = index.data(QnRecordingStatsModel::ChartData).toReal();
        QRect chartRect = opt.rect.adjusted(nx::style::Metrics::kStandardPadding, 1, -1, -1);
        chartRect.setWidth(chartRect.width() * chartData);
        painter->fillRect(chartRect, kChartBackgroundColor);
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
    const bool isNormal = (type == RowType::Normal);

    if (isNormal && index.column() == QnRecordingStatsModel::CameraNameColumn)
        return m_resourceDelegate->sizeHint(option, index);

    QSize size = base_type::sizeHint(option, index);
    if (isNormal)
        size.setWidth(std::max(size.width(), kMinColumnWidth));
    else if (type == RowType::Totals)
        size.setHeight(size.height() + kTotalLineOffset * 3);

    return size;
}

void QnRecordingStatsItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    switch (rowType(index))
    {
        case RowType::Normal:
            option->font.setWeight(kFontWeight);
            option->palette.setColor(QPalette::Text, core::colorTheme()->color("light10"));
            break;
        case RowType::Foreign:
            option->font.setWeight(kForeignCamerasFontWeight);
            option->palette.setColor(QPalette::Text,
                index.column() == QnRecordingStatsModel::CameraNameColumn
                    ? core::colorTheme()->color("dark17")
                    : core::colorTheme()->color("light10"));
                break;
        case RowType::Totals:
            option->font.setWeight(kTotalCamerasFontWeight);
            option->palette.setColor(QPalette::Text, core::colorTheme()->color("light4"));
            break;
    }

    if (index.column() == QnRecordingStatsModel::CameraNameColumn)
        option->displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    else
        option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;

    option->fontMetrics = QFontMetrics(option->font);
}
