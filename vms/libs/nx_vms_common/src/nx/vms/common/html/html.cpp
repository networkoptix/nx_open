// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "html.h"

#include <QtCore/QUrl>
#include <QtGui/QColor>
#include <QtGui/QTextDocument>
#include <QtXml/QDomDocument>

#include <nx/utils/log/assert.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>

namespace nx::vms::common {
namespace html {

namespace {

QString replaceNewLineToBrTag(const QString& text)
{
    static const auto kNewLineSymbol = '\n';
    return text.trimmed().replace(kNewLineSymbol, kLineBreak);
}

/**
 * Tries to elide node text.
 * @returns Resulting text length.
 */
int elideTextNode(QDomNode& node, int maxLength, const QString& /*tail*/)
{
    auto textNode = node.toText();
    if (textNode.isNull())
        return 0;

    const auto text = textNode.nodeValue();
    const auto textSize = text.size();
    if (textSize <= maxLength)
        return textSize;

    const auto elided = nx::utils::elideString(text, maxLength);
    textNode.setNodeValue(elided);
    return maxLength;
}

/**
 * Tries to elide node (element) text.
 * @returns Resulting text length.
 */
int elideDomNode(QDomNode& node, int maxLength, const QString& tail)
{
    // if specified node is text - elide it.
    if (auto len = elideTextNode(node, maxLength, tail))
        return len;

    int currentLength = 0;
    QList<QDomNode> forRemove;
    for (auto child = node.firstChild(); !child.isNull(); child = child.nextSibling())
    {
        if (currentLength >= maxLength)
        {
            // Removes all elements that do not fit to length.
            forRemove.append(child);
            continue;
        }

        // Tries to elide text node
        if (currentLength < maxLength)
        {
            if (auto length = elideTextNode(child, maxLength - currentLength, tail))
            {
                currentLength += length;
                continue;
            }
        }

        // if it is not text node, try to parse whole element.
        const auto elem = child.toElement();
        if (elem.isNull())
        {
            // If it is not element (comment or some specific data) - skip it.
            continue;
        }

        const int textLength = elem.text().size();
        if ((currentLength + textLength) <= maxLength)
        {
            // Text length of element (and all its child elements) is less then maximum.
            currentLength += textLength;
            continue;
        }

        currentLength += elideDomNode(child, maxLength - currentLength, tail);
    }

    // Removes all elements that do not fit.
    for (auto child: forRemove)
        node.removeChild(child);

    return currentLength;
};

QString makeLink(const QString& text, const QString& link)
{
    return nx::format("<a href=\"%1\">%2</a>", link, text).toQString();
}

QString makeLink(const QString& link)
{
    return makeLink(link, link);
}

} // namespace

Tag::Tag(const QString& tag, QString& result, LineBreaks lineBreaks):
    m_tag(tag),
    m_result(result),
    m_lineBreaks(lineBreaks)
{
    result.append("<" + m_tag + ">");
    if (m_lineBreaks.testFlag(AfterOpen))
        result.append('\n');
}

Tag::~Tag()
{
    m_result.append("</" + m_tag + ">");
    if (m_lineBreaks.testFlag(AfterClose))
        m_result.append('\n');
}

QString tagged(const QString& text, const QString& tag)
{
    return nx::format("<%1>%2</%1>", tag, text).toQString();
}

QString document(const QString& text)
{
    return tagged(text, "html");
}

QString bold(const QString& text)
{
    return tagged(text, "b");
}

QString italic(const QString& text)
{
    return tagged(text, "i");
}

QString underlined(const QString& text)
{
    return tagged(text, "u");
}

QString paragraph(const QString& text)
{
    return tagged(text, "p");
}

QString styledParagraph(const QString& text, int pixelSize, bool isBold, bool isItalic)
{
    static const QString kTag = "<p style=\" text-indent: 0;"
        " font-size: %1px; font-weight: %2; font-style: %3; color: #FFF;"
        " margin-top: 0; margin-bottom: 0; margin-left: 0; margin-right: 0; \">%4</p>";

    if (text.isEmpty())
        return QString();

    const QString boldValue(isBold ? "bold" : "normal");
    const QString italicValue(isItalic ? "italic" : "normal");

    const auto newFormattedText = replaceNewLineToBrTag(text);
    return kTag.arg(QString::number(pixelSize), boldValue, italicValue, newFormattedText);
}

QString colored(const QString& text, const QColor& color)
{
    return QString("<font color=\"%1\">%2</font>").arg(color.name(), text);
}

QString monospace(const QString& text)
{
    return QString("<font face=\"Roboto Mono\">%1</font>").arg(text);
}

QString elide(const QString& html, int maxLength, const QString& tail)
{
    QDomDocument dom;
    dom.setContent(replaceNewLineToBrTag(html));
    auto root = dom.documentElement();
    elideDomNode(root, maxLength, tail);
    return dom.toString();
}

bool mightBeHtml(const QString& text)
{
    if (!text.contains('\n'))
        return Qt::mightBeRichText(text);

    return mightBeHtml(text.split('\n'));
}

bool mightBeHtml(const QStringList& lines)
{
    return std::any_of(
        lines.cbegin(), lines.cend(), [](const QString& line) { return mightBeHtml(line); });
}

QString toHtml(const QString& source, Qt::WhiteSpaceMode whitespaceMode)
{
    return mightBeHtml(source) ? source : Qt::convertFromPlainText(source, whitespaceMode);
}

QString toPlainText(const QString& source)
{
    if (!mightBeHtml(source))
        return source;

    QTextDocument doc;
    doc.setHtml(source);
    return doc.toPlainText();
}

QString localLink(const QString& text, const QString& link)
{
    NX_ASSERT(link.startsWith("#"), "For non-standard scenarios use customLink() method.");
    return makeLink(text, link);
}

QString customLink(const QString& text, const QString& link)
{
    return makeLink(text, link);
}

QString mailtoLink(const QString& address)
{
    return nx::format("<a href=\"mailto:%1\">%1</a>", address).toQString();
}

QString link(const QUrl& url)
{
    return makeLink(url.toString());
}

QString link(const nx::utils::Url& url)
{
    return makeLink(url.toString());
}

QString link(const QString& text, const QUrl& url)
{
    return makeLink(text, url.toString());
}

QString link(const QString& text, const nx::utils::Url& url)
{
    return makeLink(text, url.toString());
}

} // namespace html
} // namespace nx::vms::common

