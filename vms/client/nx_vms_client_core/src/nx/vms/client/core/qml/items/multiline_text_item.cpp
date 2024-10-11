// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multiline_text_item.h"

#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QTextBlock>
#include <QtGui/QTextDocument>
#include <QtGui/QTextLayout>
#include <QtGui/QTextLine>
#include <QtQml/QtQml>
#include <QtQuick/QSGTextNode>

#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/common/utils/text_utils.h>
#include <nx/vms/common/html/html.h>

namespace nx::vms::client::core {

struct MultilineTextItem::Private
{
    MultilineTextItem* const q;
    QString text;
    bool isHtml = false;
    QFont font;
    QColor color;
    QTextDocument document{};
    std::unique_ptr<QTextDocument> elidedDocument; //< Created in rendering thread.
    QString elideMarker = "...";
    bool dirty = true;

    qreal calculateImplicitWidth() const
    {
        QTextDocument singleLineDoc;
        singleLineDoc.setDocumentMargin(0);
        singleLineDoc.setDefaultFont(font);
        setWrapMode(&singleLineDoc, QTextOption::NoWrap);
        setText(&singleLineDoc, text, isHtml);
        return singleLineDoc.size().width();
    }

    void updateImplicitWidth()
    {
        q->setImplicitWidth(calculateImplicitWidth());
    }

    static bool setWrapMode(QTextDocument* target, QTextOption::WrapMode value)
    {
        auto option = target->defaultTextOption();
        if (option.wrapMode() == value)
            return false;

        option.setWrapMode(value);
        target->setDefaultTextOption(option);
        return true;
    }

    static void setText(QTextDocument* target, const QString& text, bool isHtml)
    {
        if (isHtml)
            target->setHtml(text);
        else
            target->setPlainText(text);
    }
};

MultilineTextItem::MultilineTextItem(QQuickItem* parent):
    QQuickItem(parent),
    d(new Private{.q = this})
{
    setFlags(ItemHasContents);
    d->document.setDocumentMargin(0);

    connect(d->document.documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
        [this](const QSizeF& newSize) { setImplicitHeight(newSize.height()); });

    connect(&d->document, &QTextDocument::contentsChanged, this,
        [this]() { d->elidedDocument.reset(); });
}

MultilineTextItem::~MultilineTextItem()
{
    // Required here for forward-declared scoped pointer destruction.
}

QColor MultilineTextItem::color() const
{
    return d->color;
}

void MultilineTextItem::setColor(const QColor& value)
{
    if (d->color == value)
        return;

    d->color = value;
    d->dirty = true;
    update();
    emit colorChanged();
}

QString MultilineTextItem::text() const
{
    return d->text;
}

void MultilineTextItem::setText(const QString& value)
{
    if (d->text == value)
        return;

    d->text = value;
    d->isHtml = nx::vms::common::html::mightBeHtml(value);
    Private::setText(&d->document, d->text, d->isHtml);
    d->updateImplicitWidth();
    d->dirty = true;
    update();
    emit textChanged();
}

QFont MultilineTextItem::font() const
{
    return d->font;
}

void MultilineTextItem::setFont(const QFont& value)
{
    if (d->font == value)
        return;

    d->font = value;
    d->document.setDefaultFont(d->font);
    d->updateImplicitWidth();
    d->dirty = true;
    update();
    emit fontChanged();
}

MultilineTextItem::WrapMode MultilineTextItem::wrapMode() const
{
    return (WrapMode) d->document.defaultTextOption().wrapMode();
}

QString MultilineTextItem::elideMarker() const
{
    return d->elideMarker;
}

void MultilineTextItem::setElideMarker(const QString& value)
{
    if (d->elideMarker == value)
        return;

    d->elideMarker = value;
    d->elidedDocument.reset();
    d->dirty = true;
    update();
    emit elideMarkerChanged();
}

void MultilineTextItem::setWrapMode(WrapMode value)
{
    if (!Private::setWrapMode(&d->document, (QTextOption::WrapMode) value))
        return;

    d->dirty = true;
    update();
    emit wrapModeChanged();
}

void MultilineTextItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    base_type::geometryChange(newGeometry, oldGeometry);

    if (!qFuzzyEquals(newGeometry.height(), oldGeometry.height()))
    {
        d->elidedDocument.reset();
        d->dirty = true;
        update();
    }

    if (!qFuzzyEquals(newGeometry.width(), oldGeometry.width()))
    {
        d->document.setTextWidth(newGeometry.width());
        d->elidedDocument.reset();
        d->dirty = true;
        update();
    }
}

QSGNode* MultilineTextItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    auto node = static_cast<QSGTextNode*>(oldNode);
    if (!node)
    {
        node = window()->createTextNode();
        d->dirty = true;
    }

    if (d->dirty)
    {
        if (!d->elidedDocument)
        {
            d->elidedDocument.reset(d->document.clone());
            text_utils::elideDocumentHeight(d->elidedDocument.get(), height(), d->elideMarker);
        }

        node->clear();
        node->setColor(d->color);
        node->addTextDocument({0,0}, d->elidedDocument.get());
        d->dirty = false;
    }

    return node;
}

void MultilineTextItem::registerQmlType()
{
    qmlRegisterType<MultilineTextItem>("nx.vms.client.core", 1, 0, "MultilineText");
}

} // namespace nx::vms::client::core
