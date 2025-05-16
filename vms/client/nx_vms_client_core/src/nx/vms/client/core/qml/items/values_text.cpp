// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "values_text.h"

#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>
#include <QtGui/QTextFrame>
#include <QtGui/QTextTable>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGSimpleRectNode>
#include <QtQuick/QSGTextNode>
#include <QtQuick/private/qquicktext_p.h>

#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/common/utils/text_utils.h>
#include <nx/vms/client/core/skin/color_theme.h>

#include "private/color_square_text_object.h"

namespace nx::vms::client::core {

struct ValuesText::Private
{
    ValuesText* const q;
    QStringList values;
    QStringList colorValues;
    QStringList visibleValues;
    QColor color;
    QColor appendixColor;
    QString separator = ", ";
    int maximumLineCount = 1;
    QTextDocument document{};
    bool dirty = true;
    int spacing = 4;
    qreal effectiveWidth = 0;

    void updateImplicitWidth()
    {
        qreal implicitWidth = 0;
        QFontMetricsF fm(document.defaultFont());
        if (colorValues.isEmpty())
        {
            implicitWidth = fm.size(0, values.join(separator)).width();
        }
        else
        {
            for (int i = 0; i < values.count(); ++i)
            {
                if (i > 0)
                    implicitWidth += spacing;
                if (colorValues.count() > i)
                    implicitWidth += ColorSquareTextObject::sLength + spacing;
                implicitWidth += fm.size(0, values.at(i)).width();
            }
        }
        q->setImplicitWidth(implicitWidth);
    }

    QString appendixText() const
    {
        const int moreItemsCount = colorValues.count() > 0
            ? qMin(colorValues.count(), values.count()) - visibleValues.count()
            : values.count() - visibleValues.count();
        return moreItemsCount > 0
            ? QString("+%1").arg(moreItemsCount)
            : "";
    }

    qreal appendixWidth() const
    {
        QString text = appendixText();
        if (text.isEmpty())
            return 0;

        QFontMetricsF fm(document.defaultFont());
        return fm.size(0, text).width();
    }
};

ValuesText::ValuesText(QQuickItem* parent):
    base_type(parent),
    d(new Private{.q = this})
{
    setFlag(QQuickItem::ItemHasContents);
    d->document.setDocumentMargin(0.0);
}

ValuesText::~ValuesText()
{
    // Required here for forward-declared scoped pointer destruction.
}

QStringList ValuesText::values() const
{
    return d->values;
}

void ValuesText::setValues(const QStringList& values)
{
    if (d->values == values)
        return;

    d->values = values;
    d->updateImplicitWidth();
    updateItemView();
    emit valuesChanged();
}

QStringList ValuesText::colorValues() const
{
    return d->colorValues;
}

void ValuesText::setColorValues(const QStringList& values)
{
    if (d->colorValues == values)
        return;

    d->colorValues = values;
    d->updateImplicitWidth();
    updateItemView();
    emit colorValuesChanged();
}

QFont ValuesText::font() const
{
    return d->document.defaultFont();
}

void ValuesText::setFont(const QFont& value)
{
    if (d->document.defaultFont() == value)
        return;

    d->document.setDefaultFont(value);
    d->updateImplicitWidth();
    d->dirty = true;
    updateItemView();
    emit fontChanged();
}

QColor ValuesText::color() const
{
    return d->color;
}

void ValuesText::setColor(const QColor& value)
{
    if (d->color == value)
        return;

    d->color = value;
    updateItemView();
    emit colorChanged();
}

QColor ValuesText::appendixColor() const
{
    return d->appendixColor;
}

void ValuesText::setAppendixColor(const QColor& value)
{
    if (d->appendixColor == value)
        return;

    d->appendixColor = value;
    updateItemView();
    emit appendixColorChanged();
}

QString ValuesText::separator() const
{
    return d->separator;
}

void ValuesText::setSeparator(const QString& value)
{
    if (d->separator == value)
        return;

    d->separator = value;
    updateItemView();
    emit separatorChanged();
}

int ValuesText::maximumLineCount() const
{
    return d->maximumLineCount;
}

void ValuesText::setMaximumLineCount(int value)
{
    if (d->maximumLineCount == value)
        return;

    d->maximumLineCount = value;
    updateItemView();
    emit maximumLineCountChanged();
}

qreal ValuesText::effectiveWidth()
{
    return d->effectiveWidth;
}

void ValuesText::setEffectiveWidth(qreal value)
{
    if (d->effectiveWidth == value)
        return;

    d->effectiveWidth = value;
    emit effectiveWidthChanged();
}

int ValuesText::spacing() const
{
    return d->spacing;
}

void ValuesText::setSpacing(int value)
{
    if (d->spacing == value)
        return;

    d->spacing = value;
    updateItemView();
    emit spacingChanged();
}

void ValuesText::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    base_type::geometryChange(newGeometry, oldGeometry);
    if (!qFuzzyEquals(newGeometry.width(), oldGeometry.width()))
    {
        d->document.setTextWidth(newGeometry.width());
        updateItemView();
    }
}

QSGNode* ValuesText::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* /*data*/)
{
    auto node = static_cast<QSGTextNode*>(oldNode);
    if (!node)
    {
        node = window()->createTextNode();
        d->dirty = true;
    }

    if (d->dirty)
    {
        node->clear();
        node->addTextDocument({0,0}, &d->document);
        d->dirty = false;
    }

    return node;
}

void ValuesText::updateItemView()
{
    d->document.clear();
    if (!d->values.isEmpty())
    {
        d->visibleValues.clear();
        if (d->colorValues.isEmpty())
            updateTextRow();
        else
            updateColorRow();
    }
    d->document.documentLayout();
    d->dirty = true;
    update();
}

void ValuesText::updateTextRow()
{
    QTextLayout layout;
    layout.setFont(d->document.defaultFont());

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    qreal textHeight = 0;
    qreal availableWidth = width();
    qreal appendixWidth = 0;

    d->visibleValues = d->values;
    while (d->visibleValues.count() > 0)
    {
        appendixWidth = d->appendixWidth();
        availableWidth = width() - appendixWidth - (appendixWidth > 0 ? d->spacing : 0);
        layout.setText(d->visibleValues.join(d->separator));
        textHeight = 0;
        layout.beginLayout();
        while (true)
        {
            QTextLine line = layout.createLine();
            if (!line.isValid() || layout.lineCount() > d->maximumLineCount)
                break;

            line.setLineWidth(availableWidth);
            textHeight += line.height();
        }
        layout.endLayout();
        if (d->visibleValues.count() == 1 || layout.lineCount() <= d->maximumLineCount)
            break;

        d->visibleValues.removeLast();
    }
    setImplicitHeight(textHeight);

    QTextCursor cursor(&d->document);
    const QTextTable* table = nullptr;
    const QString appendix = d->appendixText();
    if (!appendix.isEmpty())
    {
        QTextTableFormat tableFormat;
        tableFormat.setCellPadding(0);
        tableFormat.setCellSpacing(0);
        tableFormat.setBorder(0);
        tableFormat.setWidth(width());
        tableFormat.setHeight(textHeight);
        QList<QTextLength> constraints = {
            QTextLength(QTextLength::FixedLength, ceil(availableWidth)),
            QTextLength(),
            QTextLength(QTextLength::FixedLength, ceil(appendixWidth))
        };
        tableFormat.setColumnWidthConstraints(constraints);
        table = cursor.insertTable(1, 3, tableFormat);
        cursor = table->cellAt(0, 0).firstCursorPosition();
    }
    QTextCharFormat textFormat;
    textFormat.setForeground(d->color);
    cursor.setCharFormat(textFormat);

    qreal effectiveWidth = 0;
    QFontMetricsF fm(d->document.defaultFont());
    for (int i = 0; i < layout.lineCount(); ++i)
    {
        const QTextLine line = layout.lineAt(i);
        QString text = layout.text().mid(line.textStart(), line.textLength());
        if (i == d->maximumLineCount - 1 && layout.lineCount() > d->maximumLineCount)
        {
            const QTextLine nextLine = layout.lineAt(i + 1);
            text.append(layout.text().mid(nextLine.textStart(), nextLine.textLength()));
            text = fm.elidedText(text, Qt::ElideRight, floor(availableWidth), Qt::TextSingleLine);
            effectiveWidth = qMax(effectiveWidth, fm.size(0, text).width());
            cursor.insertText(text);
            break;
        }
        effectiveWidth = qMax(effectiveWidth, fm.size(0, text).width());
        cursor.insertText(text);
    }

    if (!appendix.isEmpty())
    {
        auto cell = table->cellAt(0, 2);
        QTextTableCellFormat cellFormat = cell.format().toTableCellFormat();
        cellFormat.setVerticalAlignment(QTextCharFormat::AlignMiddle);
        cell.setFormat(cellFormat);

        textFormat.setForeground(d->appendixColor);
        cursor = cell.firstCursorPosition();
        cursor.setCharFormat(textFormat);
        cursor.insertText(appendix);

        effectiveWidth += appendixWidth + d->spacing;
    }
    setEffectiveWidth(effectiveWidth);
}

void ValuesText::updateColorRow()
{
    auto squareInterface = new ColorSquareTextObject;
    squareInterface->setParent(&d->document);
    d->document.documentLayout()->registerHandler(
        ColorSquareTextObject::ColorSquareTextFormat, squareInterface);

    qreal valuesWidth = 0;
    qreal appendixWidth = 0;
    qreal availableWidth = 0;
    QFontMetricsF fm(d->document.defaultFont());
    d->visibleValues = d->values;
    while (d->visibleValues.count() > 0)
    {
        valuesWidth = 0;
        appendixWidth = d->appendixWidth();
        availableWidth = width() - (appendixWidth > 0 ? appendixWidth + d->spacing : 0);
        for (int i = 0; i < d->visibleValues.count(); ++i)
        {
            if (valuesWidth > 0)
                valuesWidth += d->spacing;
            if (d->colorValues.count() > i)
                valuesWidth += ColorSquareTextObject::sLength + d->spacing;
            valuesWidth += fm.size(0, d->visibleValues.at(i)).width();
        }
        if (d->visibleValues.count() == 1 || valuesWidth <= availableWidth)
            break;
        d->visibleValues.removeLast();
    }
    setImplicitHeight(ColorSquareTextObject::sLength);

    QTextCursor cursor(&d->document);
    QTextCharFormat textFormat;
    textFormat.setForeground(d->color);
    textFormat.setVerticalAlignment(QTextCharFormat::AlignTop);
    cursor.setCharFormat(textFormat);

    const QString appendix = d->appendixText();
    QTextTableFormat tableFormat;
    tableFormat.setCellPadding(0);
    tableFormat.setCellSpacing(0);
    tableFormat.setBorder(0);
    tableFormat.setWidth(width());
    QList<QTextLength> constraints;
    for (int i = 0; i < d->visibleValues.count(); ++i)
        constraints.append(QTextLength());
    qreal spacerWidth = width() - valuesWidth - appendixWidth;
    if (spacerWidth > 0)
        constraints.append(QTextLength(QTextLength::FixedLength, spacerWidth));
    if (!appendix.isEmpty())
        constraints.append(QTextLength(QTextLength::FixedLength, ceil(appendixWidth)));
    tableFormat.setColumnWidthConstraints(constraints);
    auto table = cursor.insertTable(1, constraints.count(), tableFormat);

    for (int i = 0; i < d->visibleValues.count(); ++i)
    {
        cursor = table->cellAt(0, i).firstCursorPosition();
        int decorationWidth = 0;
        if (d->colorValues.count() > i)
        {
            QColor color(d->colorValues.at(i));
            QTextCharFormat squareFormat(textFormat);
            squareFormat.setObjectType(ColorSquareTextObject::ColorSquareTextFormat);
            squareFormat.setProperty(ColorSquareTextObject::Color, color);
            cursor.insertText(QString(QChar::ObjectReplacementCharacter) + " ", squareFormat);
            decorationWidth = ColorSquareTextObject::sLength + d->spacing;
        }
        QString text = d->values.at(i);
        if (d->visibleValues.count() == 1)
        {
            if (availableWidth >= decorationWidth)
                text = fm.elidedText(text, Qt::ElideRight, availableWidth - decorationWidth);
            else
                text = "";
        }
        cursor.insertText(text);
    }
    if (appendixWidth > 0)
    {
        auto cell = table->cellAt(0, constraints.count() - 1);
        cursor = cell.firstCursorPosition();
        textFormat.setForeground(d->appendixColor);
        cursor.setCharFormat(textFormat);
        cursor.insertText(appendix);
    }
    setEffectiveWidth(valuesWidth + (appendixWidth > 0 ? appendixWidth + d->spacing : 0));
}

void ValuesText::registerQmlType()
{
    qmlRegisterType<ValuesText>("Nx.Core", 1, 0, "ValuesText");
}

} // namespace nx::vms::client::core
