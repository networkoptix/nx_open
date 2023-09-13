// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

class QColor;
class QUrl;

namespace nx::utils { class Url; }

/**
 * Set of functions for generating Qt-compliant html-formatted text lines.
 */
namespace nx::vms::common {
namespace html {

static constexpr auto kLineBreak("<br/>");
static constexpr auto kHorizontalLine("<hr/>");

/** RAII helper to consistently open/close html tags on the given string. */
class NX_VMS_COMMON_API Tag
{
public:
    enum LineBreak
    {
        NoBreaks = 0x00,
        AfterOpen = 0x01,
        AfterClose = 0x02,
        BothBreaks = AfterOpen | AfterClose
    };
    Q_DECLARE_FLAGS(LineBreaks, LineBreak)

    Tag(const QString& tag, QString& result, LineBreaks lineBreaks = AfterClose);
    ~Tag();
private:
    QString m_tag;
    QString& m_result;
    LineBreaks m_lineBreaks;
};

/** Enclose given text in a given tag. */
NX_VMS_COMMON_API QString tagged(
    const QString& text, const QString& tag, const QString& attributes = "");

/** Enclose given text in a given tag if text is not empty. */
NX_VMS_COMMON_API QString taggedIfNotEmpty(
    const QString& text, const QString& tag, const QString& attributes = "");

/** <html>text</html> */
NX_VMS_COMMON_API QString document(const QString& text);

/** <b>text</b> */
NX_VMS_COMMON_API QString bold(const QString& text);

/** <i>text</i> */
NX_VMS_COMMON_API QString italic(const QString& text);

/** <u>text</u> */
NX_VMS_COMMON_API QString underlined(const QString& text);

/** <p>text</p> */
NX_VMS_COMMON_API QString paragraph(const QString& text);

NX_VMS_COMMON_API QString styledParagraph(
    const QString& text,
    int pixelSize,
    bool isBold = false,
    bool isItalic = false);

NX_VMS_COMMON_API QString colored(const QString& text, const QColor& color);

NX_VMS_COMMON_API QString monospace(const QString& text);

NX_VMS_COMMON_API QString elide(const QString& html, int maxLength, const QString& tail = "...");

/** Check if the given text might be html-formatted. */
NX_VMS_COMMON_API bool mightBeHtml(const QString& text);

/** Check if the given text might be html-formatted. */
NX_VMS_COMMON_API bool mightBeHtml(const QStringList& lines);

/** Converts plain text to HTML if needed. */
NX_VMS_COMMON_API QString toHtml(
    const QString& source, Qt::WhiteSpaceMode whitespaceMode = Qt::WhiteSpaceNormal);

/** Converts HTML to plain text if needed. */
NX_VMS_COMMON_API QString toPlainText(const QString& source);

/**
 * Create html link with the given text and anchor.
 */
NX_VMS_COMMON_API QString localLink(const QString& text, const QString& link = "#");

/**
 * Create html link with the given text and link (neither local nor valid url).
 */
NX_VMS_COMMON_API QString customLink(const QString& text, const QString& link);

/**
 * Create mailto link with text, equal to address.
 */
NX_VMS_COMMON_API QString mailtoLink(const QString& address);

/**
 * Create html link with text, equal to url.
 */
NX_VMS_COMMON_API QString link(const QUrl& url);

/**
 * Create html link with text, equal to url.
 */
NX_VMS_COMMON_API QString link(const nx::utils::Url& url);

/**
 * Create html link with the given text and url.
 */
NX_VMS_COMMON_API QString link(const QString& text, const QUrl& url);

/**
 * Create html link with the given text and url.
 */
NX_VMS_COMMON_API QString link(const QString& text, const nx::utils::Url& url);

/**
 * Return text tagged as no-wrap.
 */
NX_VMS_COMMON_API QString noWrap(const QString& text);

/**
 * Extended analog of QString::toHtmlEscaped() which also affect spaces.
 */
NX_VMS_COMMON_API QString toHtmlEscaped(const QString& text);

/**
 * Marks all matches substrings from regular expression \a rx in \a text with \a color
 */
NX_VMS_COMMON_API QString highlightMatch(
    const QString& text, const QRegularExpression& rx, const QColor& color);

} // namespace html
} // namespace nx::vms::common
