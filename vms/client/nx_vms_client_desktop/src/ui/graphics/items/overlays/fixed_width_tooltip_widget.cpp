// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fixed_width_tooltip_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QTextCursor>

#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>

QnFixedWidthTooltipWidget::QnFixedWidthTooltipWidget(QGraphicsItem* parent): base_type(parent)
{
    m_textDocument.setDocumentMargin(0);
    m_textDocument.setIndentWidth(0);
}

QString QnFixedWidthTooltipWidget::text() const
{
    return m_textDocument.toPlainText();
}

void QnFixedWidthTooltipWidget::setText(const QString& text, qreal lineHeight /* = 100 */)
{
    if (text != m_text || lineHeight != m_lineHeight)
    {
        m_text = text;
        m_lineHeight = lineHeight;
        generateTextPixmap();
        updateHeight();
    }
}

void QnFixedWidthTooltipWidget::setTextWidth(qreal width)
{
    if (!qFuzzyCompare(width, m_textWidth))
    {
        m_textWidth = width;
        m_textDocument.setTextWidth(width);

        const auto& margins = contentsMargins();
        auto newGeometry = geometry();
        auto widgetWidth = margins.left() + margins.right() + width;
        newGeometry.setWidth(widgetWidth);
        setPreferredWidth(widgetWidth);
        setMinimumWidth(widgetWidth);
        setGeometry(newGeometry);

        generateTextPixmap();
        updateHeight();
    }
}

void QnFixedWidthTooltipWidget::setTextColor(const QColor& color)
{
    if (color != m_textColor)
    {
        m_textColor = color;
        generateTextPixmap();
        updateHeight();
    }
}

void QnFixedWidthTooltipWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    base_type::paint(painter, option, widget);

    paintSharp(painter,
        [this, option, widget](QPainter* painter)
        {
            const auto& margins = contentsMargins();
            painter->drawPixmap(margins.left(), margins.top(), m_textPixmap);
        });
}

void QnFixedWidthTooltipWidget::updateHeight()
{
    auto margins = contentsMargins();
    auto textHeight = m_textDocument.size().height();
    auto newHeight = margins.top() + margins.bottom() + textHeight;
    if (newHeight != m_oldHeight)
    {
        m_oldHeight = newHeight;
        auto newGeometry = geometry();
        newGeometry.setHeight(newHeight);
        setGeometry(newGeometry);
        update();
    }
}

void QnFixedWidthTooltipWidget::generateTextPixmap()
{
    m_textDocument.setDefaultStyleSheet(
        QStringLiteral("body { color: %1; background-color: transparent; }")
            .arg(m_textColor.name()));
    m_textDocument.setHtml(QStringLiteral("<body>%1</body>").arg(m_text));

    QTextCursor cursor(&m_textDocument);
    cursor.select(QTextCursor::Document);

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(m_lineHeight, QTextBlockFormat::ProportionalHeight);
    cursor.setBlockFormat(blockFormat);

    QTextOption option = m_textDocument.defaultTextOption();
    option.setAlignment(Qt::AlignCenter);
    m_textDocument.setDefaultTextOption(option);

    auto ratio = qApp->devicePixelRatio();
    const auto& size = m_textDocument.size() * ratio;

    if (size.isValid())
    {
        m_textPixmap = QPixmap(size.toSize());
        m_textPixmap.setDevicePixelRatio(ratio);
        m_textPixmap.fill(Qt::transparent);

        QPainter painter(&m_textPixmap);
        paintSharp(&painter,
            [this](QPainter* painter)
            {
                QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
                m_textDocument.drawContents(painter);
            });
    }
}
