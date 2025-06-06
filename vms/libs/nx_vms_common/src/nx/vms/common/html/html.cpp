// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "html.h"

#include <QtCore/QRegularExpression>
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

QString tagged(const QString& text, const QString& tag, const QString& attributes)
{
    return attributes.isEmpty()
        ? nx::format("<%1>%2</%1>", tag, text).toQString()
        : nx::format("<%1 %2>%3</%1>", tag, attributes, text).toQString();
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

QString underlinedColored(const QString& text, const QColor& color)
{
    static const QString kTag =
        "<span style=\"text-decoration:underline;text-decoration-color:%1;\">%2</span>";
    if (text.isEmpty())
        return QString();

    return kTag.arg(color.name(), text);
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

    const auto newFormattedText = mightBeHtml(text) ? text : replaceNewLineToBrTag(text);
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
    dom.setContent(mightBeHtml(html) ? html : replaceNewLineToBrTag(html));
    auto root = dom.documentElement();
    elideDomNode(root, maxLength, tail);
    return dom.toString();
}

bool mightBeHtml(const QString& text)
{
    if (!text.contains('\n'))
        return Qt::mightBeRichText(text)
            || text.contains(QRegularExpression("&amp;|&lt;|&gt;|&quot;|&nbsp;"));

    return mightBeHtml(text.split('\n'));
}

bool mightBeHtml(const QStringList& lines)
{
    return std::any_of(
        lines.cbegin(), lines.cend(), [](const QString& line) { return mightBeHtml(line); });
}

QString toHtml(const QString& source, Qt::WhiteSpaceMode whitespaceMode)
{
    if (source.isEmpty())
        return {};

    // Change <br> to <br/>, as they are not fully valid XML.
    // QDomDocument requires strict XML compliance and will reject such tags.
    return mightBeHtml(source)
        ? source
        : Qt::convertFromPlainText(source, whitespaceMode).replace("<br>", kLineBreak);
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

QString phoneNumberLink(const QString& phoneNumber)
{
    return nx::format("<a href=\"tel:%1\">%1</a>", phoneNumber).toQString();
}

QString link(const QUrl& url)
{
    return makeLink(url.toString());
}

QString link(const nx::Url& url)
{
    return makeLink(url.toString());
}

QString link(const QString& text, const QUrl& url)
{
    return makeLink(text, url.toString());
}

QString link(const QString& text, const nx::Url& url)
{
    return makeLink(text, url.toString());
}

QString toHtmlEscaped(const QString& text, EscapeMethod escapeMethod)
{
    static const QString kLt("&lt;");
    static const QString kGt("&gt;");
    static const QString kAmp("&amp;");
    static const QString kQuot("&quot;");
    static const QString kNbsp("&nbsp;");

    auto indexOfAny =
        [escapeMethod](const QStringView& s, int pos, const QString** replacement)
        {
            const auto view = s.mid(pos);
            for (const QChar& c: view)
            {
                switch (c.unicode())
                {
                    case '<':
                        *replacement = &kLt;
                        return pos;
                    case '>':
                        *replacement = &kGt;
                        return pos;
                    case '&':
                        *replacement = &kAmp;
                        return pos;
                    case '"':
                        *replacement = &kQuot;
                        return pos;
                    case ' ':
                        if (escapeMethod == EscapeMethod::nbsps)
                        {
                            *replacement = &kNbsp;
                            return pos;
                        }
                }
                ++pos;
            }

            replacement = nullptr;
            return -1;
        };

    auto wrapper = [escapeMethod](const QString& text)
    {
        if (text.isEmpty())
            return QString();

        if (escapeMethod == EscapeMethod::preWrap)
            return tagged(text, "div", "style=\"white-space: pre-wrap\"");

        if (escapeMethod == EscapeMethod::noWrap)
            return tagged(text, "div", "style=\"white-space: nowrap\"");

        return text;
    };

    const QString* replacement = nullptr;

    int pos = 0;
    int nextPos = indexOfAny(text, pos, &replacement);
    if (nextPos == -1)
        return wrapper(text);

    QString result;
    result.reserve(static_cast<qsizetype>(text.length() * 1.1));

    do
    {
        result += QStringView(text).mid(pos, nextPos - pos);
        result += *replacement;
        pos = nextPos + 1;
        nextPos = indexOfAny(text, pos, &replacement);
    } while (nextPos != -1);

    if (pos < text.length())
        result.append(QStringView(text).mid(pos));

    result.squeeze();
    return wrapper(result);
}

QString highlightMatch(const QString& text, const QRegularExpression& rx, const QColor& color)
{
    QString result;
    if (!text.isEmpty())
    {
        if (!rx.isValid() || rx.pattern().isEmpty())
            return toHtmlEscaped(text);

        const QString fontBegin = QString("<font color=\"%1\">").arg(color.name());
        static const QString fontEnd = "</font>";
        int pos = 0;
        auto matchIterator = rx.globalMatch(text);
        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            if (match.capturedLength() == 0)
                break;

            result.append(toHtmlEscaped(text.mid(pos, match.capturedStart() - pos)));
            result.append(fontBegin);
            result.append(toHtmlEscaped(match.captured()));
            result.append(fontEnd);
            pos = match.capturedEnd();
        }
        result.append(toHtmlEscaped(text.mid(pos)));
    }

    return result;
}

} // namespace html
} // namespace nx::vms::common
