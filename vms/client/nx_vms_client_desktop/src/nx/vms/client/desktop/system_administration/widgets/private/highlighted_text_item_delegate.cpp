// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "highlighted_text_item_delegate.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractItemView>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

namespace nx::vms::client::desktop {

namespace {

QString highlightMatch(const QString& text, const QRegularExpression& rx, const QColor& color)
{
    QString result;
    if (!text.isEmpty())
    {
        const QString fontBegin = QString("<font color=\"%1\">").arg(color.name());
        static const QString fontEnd = "</font>";
        int pos = 0;
        auto matchIterator = rx.globalMatch(text);
        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            if (match.capturedLength() == 0)
                break;

            result.append(text.mid(pos, match.capturedStart() - pos).toHtmlEscaped());
            result.append(fontBegin);
            result.append(match.captured().toHtmlEscaped());
            result.append(fontEnd);
            pos = match.capturedEnd();
        }
        result.append(text.mid(pos).toHtmlEscaped());
    }

    return result;
}

} // namespace

HighlightedTextItemDelegate::HighlightedTextItemDelegate(
    QObject* parent,
    const QSet<int>& colums)
    :
    base_type(parent),
    m_columns(colums)
{
}

QSize HighlightedTextItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!m_columns.empty() && !m_columns.contains(index.column()))
        return base_type::sizeHint(option, index);

    auto opt = option;
    initStyleOption(&opt, index);

    QTextDocument doc;
    initTextDocument(doc, opt);

    return QSize(
        qCeil(doc.idealWidth()) + 2 * style::Metrics::kStandardPadding,
        base_type::sizeHint(option, index).height());
}

void HighlightedTextItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!m_columns.empty() && !m_columns.contains(index.column()))
    {
        base_type::paint(painter, option, index);
        return;
    }

    auto opt = option;
    initStyleOption(&opt, index);

    QTextDocument doc;
    initTextDocument(doc, opt);

    painter->save();

    opt.rect = opt.rect.adjusted(
        style::Metrics::kStandardPadding, 0, -style::Metrics::kStandardPadding, 0);

    WidgetUtils::elideTextRight(&doc, opt.rect.width());

    const auto rect = QStyle::alignedRect(
        Qt::LeftToRight,
        Qt::AlignLeft | Qt::AlignVCenter,
        doc.size().toSize(),
        opt.rect);

    painter->translate(rect.left(), rect.top());
    const QRect clip(0, 0, rect.width(), rect.height());

    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette = opt.palette;

    painter->setClipRect(clip);
    ctx.clip = clip;

    doc.documentLayout()->draw(painter, ctx);

    painter->restore();
}

void HighlightedTextItemDelegate::initTextDocument(
    QTextDocument& doc,
    const QStyleOptionViewItem& option) const
{
    auto opt = option;

    QRegularExpression rx;
    if (const auto itemView = qobject_cast<const QAbstractItemView*>(opt.widget))
    {
        if (const auto m = qobject_cast<const QSortFilterProxyModel*>(itemView->model()))
            rx = m->filterRegularExpression();
    }

    doc.setDefaultFont(opt.font);
    doc.setDocumentMargin(0);

    doc.setHtml(highlightMatch(opt.text, rx, core::colorTheme()->color("yellow_d1")));
}

} // namespace nx::vms::client::desktop
