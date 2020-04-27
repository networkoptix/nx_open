#pragma once

#include <QByteArray>
#include <QString>

/** Trim each line in a multiline QByteArray and remove all empty lines after trimming. */
[[nodiscard]] QByteArray linesTrimmed(const QByteArray& document);
/** Trim each line in a multiline QString and remove all empty lines after trimming. */
[[nodiscard]] QString linesTrimmed(const QString& document);

/** User literal to make QByteArray from string literal (applies `linesTrimmed` inside). */
[[nodiscard]] QByteArray operator""_linesTrimmed(const char* data, size_t size);
/** User literal to make QStirng from utf16 string literal (applies `linesTrimmed` inside). */
[[nodiscard]] QString operator""_linesTrimmed(const char16_t* data, size_t size);
