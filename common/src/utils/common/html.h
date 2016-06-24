#pragma once

#include <QtCore/QString>
#include <QtCore/QUrl>

QString htmlBold(const QString &source);
QString htmlFormattedParagraph(const QString &text
    , int pixelSize
    , bool isBold = false
    , bool isItalic = false);

QString makeHref(const QString &text, const QUrl &url);

QString escapeHtml(const QString& input);

QString unescapeHtml(const QString& escaped);

QString elideHtml(const QString &html, int maxLength, const QString &tail = lit("..."));
