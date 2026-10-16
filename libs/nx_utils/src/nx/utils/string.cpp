// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include <QtCore/QByteArray>
#include <QtCore/QRegularExpression>

#include <nx/kit/utils.h>
#include <nx/utils/datetime.h>
#include <nx/utils/exception.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>

using nx::kit::utils::isSpaceOrControlChar;

namespace nx::utils {

QString replaceCharacters(
    const QString &string,
    std::string_view symbols,
    const QChar &replacement)
{
    if (symbols.empty())
        return string;

    bool mask[256];
    memset(mask, 0, sizeof(mask));
    for (const auto s: symbols)
        mask[static_cast<int>(s)] = true; /* Static cast is here to silence GCC's -Wchar-subscripts. */

    QString result = string;
    for (int i = 0; i < result.size(); i++)
    {
        const ushort c = result[i].unicode();
        if (c >= 256 || !mask[c])
            continue;

        result[i] = replacement;
    }

    return result;
}

int parseInt(const QString& string, int base)
{
    int value;
    if (bool ok; (value = string.toInt(&ok, base)), !ok)
    {
        if (base == 10)
            throw ContextedException("Failed to parse int: %1", string);
        else
            throw ContextedException("Failed to parse base-%1 int: %2", base, string);
    }
    return value;
}

double parseDouble(const QString& string)
{
    int value;
    if (bool ok; (value = string.toDouble(&ok)), !ok)
        throw ContextedException("Failed to parse double: %1", string);
    return value;
}

QString xorEncrypt(const QString &plaintext, const QString &key)
{
    if (key.isEmpty())
        return plaintext;

    QByteArray array(plaintext.toUtf8());
    QByteArray keyArray(key.toUtf8());

    for (int i = 0; i < array.size(); i++)
        array[i] = array[i] ^ keyArray[i % keyArray.size()];

    return QLatin1String(array.toBase64());
}

QString xorDecrypt(const QString &crypted, const QString &key)
{
    if (key.isEmpty())
        return crypted;

    QByteArray array = QByteArray::fromBase64(crypted.toLatin1());
    QByteArray keyArray(key.toUtf8());

    for (int i = 0; i < array.size(); i++)
        array[i] = array[i] ^ keyArray[i % keyArray.size()];
    return QString::fromUtf8(array);
}

QString extractFileExtension(const QString &string)
{
    auto pos = string.lastIndexOf('.');
    if (pos < 0)
        return QString();

    QString result('.');
    while (++pos < string.length())
    {
        QChar curr = string[pos];
        if (!curr.isLetterOrNumber())
            return result;
        result.append(curr);
    }

    return result;
}

QString generateUniqueString(
    const QStringList& usedStrings,
    const QString& defaultString,
    const QString& templateString)
{
    QStringList lowerStrings;
    for (const QString &string : usedStrings)
        lowerStrings << string.toLower();

    const QString escapedTemplate = replaceStrings(templateString,
        {{"(", "\\("}, {")", "\\)"}});
    const QString stringPattern = escapedTemplate.arg("?([0-9]+)?").toLower();
    auto pattern = QRegularExpression(QRegularExpression::anchoredPattern(stringPattern));

    /* Prepare new name. */
    int number = 0;
    for (const QString &string : lowerStrings)
    {
        auto result = pattern.match(string);
        if (!result.hasMatch())
            continue;

        number = qMax(number, result.capturedView(1).toInt());
    }

    if (number == 0)
    {
        if (defaultString.isEmpty())
        {
            number = 1;
        }
        else if (!lowerStrings.contains(defaultString.toLower()))
        {
            return defaultString;
        }
        else
        {
            number = 2;
        }
    }
    else
    {
        number++;
    }

    return templateString.arg(number);
}

// -------------------------------------------------------------------------- //
// String comparison
// -------------------------------------------------------------------------- //

namespace {

bool isDigit(QChar c)
{
    return c.isDigit();
}

bool isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

QChar toLower(QChar c)
{
    return c.toLower();
}

char toLower(char c)
{
    return (char) std::tolower(static_cast<unsigned char>(c));
}

QStringView left(QStringView view, std::ptrdiff_t count)
{
    return view.left(count);
}

std::string_view left(std::string_view view, std::ptrdiff_t count)
{
    return view.substr(0, count);
}

QStringView dropFirst(QStringView view, std::ptrdiff_t count)
{
    return view.sliced(count);
}

std::string_view dropFirst(std::string_view view, std::ptrdiff_t count)
{
    view.remove_prefix(count);
    return view;
}

int toNumber(QStringView sv)
{
    return sv.toInt();
}

int toNumber(std::string_view sv)
{
    int value = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), value);
    return value;
}

template<typename TChar>
class Token
{
public:
    Token(int value): m_value(value) {}

    Token(Qt::CaseSensitivity caseSensitive, TChar c):
        m_value(caseSensitive == Qt::CaseSensitive ? c : toLower(c))
    {
    }

    friend bool operator==(const Token& l, const Token& r) { return l.m_value == r.m_value; }
    friend bool operator<(const Token& l, const Token& r) { return l.m_value < r.m_value; }

private:
    std::variant<int, TChar> m_value;
};

template<typename View>
auto takeFirstToken(View& string, Qt::CaseSensitivity caseSensitive)
{
    using TChar = std::decay_t<decltype(string[0])>;

    if (!isDigit(string[0]))
    {
        const TChar c = string[0];
        string = dropFirst(string, 1);
        return Token<TChar>(caseSensitive, c);
    }

    const auto endOfNumber = std::ranges::find_if_not(string, [](auto c) { return isDigit(c); });
    const auto numberView = left(string, endOfNumber - string.begin());
    string = dropFirst(string, numberView.size());
    return Token<TChar>(toNumber(numberView));
}

template<typename View>
int naturalStringCompareImpl(View lhs, View rhs, Qt::CaseSensitivity caseSensitive)
{
    while (!lhs.empty() && !rhs.empty())
    {
        const auto lhsToken = takeFirstToken(lhs, caseSensitive);
        const auto rhsToken = takeFirstToken(rhs, caseSensitive);
        if (lhsToken < rhsToken)
            return -1;
        if (rhsToken < lhsToken)
            return 1;
    }

    if (!lhs.empty())
        return 1;
    if (!rhs.empty())
        return -1;
    return 0;
}

} // namespace

int naturalStringCompare(QStringView lhs, QStringView rhs, Qt::CaseSensitivity caseSensitive)
{
    return naturalStringCompareImpl(lhs, rhs, caseSensitive);
}

int naturalStringCompare(
    std::string_view lhs, std::string_view rhs, Qt::CaseSensitivity caseSensitive)
{
    return naturalStringCompareImpl(lhs, rhs, caseSensitive);
}

bool naturalStringLess(const QString& lhs, const QString& rhs)
{
    return naturalStringCompare(lhs, rhs, Qt::CaseInsensitive) < 0;
}

bool naturalStringLess(std::string_view lhs, std::string_view rhs)
{
    return naturalStringCompare(lhs, rhs, Qt::CaseInsensitive) < 0;
}

QStringList naturalStringSort(const QStringList& list, Qt::CaseSensitivity caseSensitive)
{
    QStringList result = list;
    std::ranges::sort(result,
        [caseSensitive](const QString& left, const QString& right)
        { return naturalStringCompare(left, right, caseSensitive) < 0; });
    return result;
}

void trimInPlace(QString* const str, const QString& symbols)
{
    int startPos = 0;
    for (; startPos < str->size(); ++startPos)
    {
        if (!symbols.contains(str->at(startPos)))
            break;
    }

    int endPos = str->size() - 1;
    for (; endPos >= 0; --endPos)
    {
        if (!symbols.contains(str->at(endPos)))
            break;
    }
    ++endPos;

    *str = str->mid(startPos, endPos > startPos ? (endPos - startPos) : 0);
}

QString elideString(const QString &source, int maxLength, const QString &tail)
{
    if (source.length() <= maxLength)
        return source;

    const auto tailLength = tail.length();
    const auto elidedText = source.left(maxLength > tailLength ? maxLength - tailLength : 0);
    return (elidedText + tail);
}

QString replaceStrings(
    const QString& source,
    const std::vector<std::pair<QString, QString>>& substitutions,
    Qt::CaseSensitivity caseSensitivity)
{
    if (substitutions.empty() || source.isEmpty())
        return source;

    QHash<QString, QString> replacements;
    QString pattern;
    for (const auto& [src, dst]: substitutions)
    {
        if (!replacements.contains(src))
            replacements[src] = dst;

        pattern.append(QRegularExpression::escape(src));
        pattern.append('|');
    }
    pattern.chop(1);

    QRegularExpression re(pattern,
        caseSensitivity == Qt::CaseInsensitive
            ? QRegularExpression::CaseInsensitiveOption
            : QRegularExpression::NoPatternOption);

    QString result;

    int lastPos = 0;
    auto it = re.globalMatch(source);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        result += source.mid(lastPos, match.capturedStart() - lastPos);
        result += replacements.value(match.captured());
        lastPos = match.capturedEnd();
    }
    result += source.mid(lastPos);

    return result;
}

std::string generateRandomName(int length)
{
    return random::generateName(length);
}

static const double kByteSuffixLimit = 1024;
static const std::vector<char> kByteSuffexes = { 'K', 'M', 'G', 'T' };

// TODO: #sivanov Move out strings and logic to separate class.
std::string bytesToString(uint64_t bytes, int precision)
{
    double number = static_cast<double>(bytes);
    size_t suffix = 0;

    while (number >= kByteSuffixLimit
           && suffix < kByteSuffexes.size())
    {
        number /= kByteSuffixLimit;
        ++suffix;
    }

    // TODO: #akolesnikov Use std::to_chars when available on every platform.

    if (suffix == 0)
        return nx::format("%1").arg(number, 0, 'g', precision).toStdString();

    return nx::format("%1%2").arg(number, 0, 'g', precision).arg(kByteSuffexes[suffix - 1]).toStdString();
}

uint64_t stringToBytes(std::string_view str, bool* isOk)
{
    if (str.empty())
    {
        if (isOk)
            *isOk = false;
        return 0;
    }

    const auto subDouble = [isOk](std::string_view str, const auto& multi)
    {
        std::size_t endPos = 0;
        const auto result = static_cast<uint64_t>(stod(str, &endPos) * multi);
        if (isOk)
            *isOk = endPos > 0;
        return result;
    };

    for (size_t i = 0; i < kByteSuffexes.size(); ++i)
    {
        if (toupper(str.back()) == toupper(kByteSuffexes[i]))
        {
            return subDouble(
                str.substr(0, str.size() - 1),
                std::pow(kByteSuffixLimit, static_cast<double>(i + 1)));
        }
    }

    // NOTE: avoid double to be the most precise
    std::size_t endPos = 0;
    const auto result = stoull(str, &endPos);
    if (isOk)
        *isOk = endPos > 0;
    return result;
}

uint64_t stringToBytes(std::string_view str, uint64_t defaultValue)
{
    bool isOk = false;
    uint64_t value = stringToBytes(str, &isOk);
    if (isOk)
        return value;
    else
        return defaultValue;
}

QString removeMnemonics(QString text)
{
    /**
     * Regular expression is:
     * (not match '&') match '&' (not match whitespace or '&')
     */
    return text.remove(QRegularExpression(QStringLiteral("(?<!&)&(?!([\\s&]|$))")));
}

QString escapeMnemonics(QString text)
{
    return text.replace("&","&&");
}

template <class T, class T2>
static QList<T> smartSplitInternal(
    const T& data,
    const T2 delimiter,
    const T2 quoteChar,
    bool keepEmptyParts)
{
    bool quoted = false;
    QList<T> rez;
    if (data.isEmpty())
        return rez;

    int lastPos = 0;
    for (int i = 0; i < data.size(); ++i)
    {
        if (data[i] == quoteChar)
            quoted = !quoted;
        else if (data[i] == delimiter && !quoted)
        {
            T value = data.mid(lastPos, i - lastPos);
            if (!value.isEmpty() || keepEmptyParts)
                rez << value;
            lastPos = i + 1;
        }
    }
    int end = data.size();
    if (!keepEmptyParts && quoted)
    {
        while (end > lastPos && data[end - 1] == delimiter)
            --end;
    }
    if (keepEmptyParts || end > lastPos)
        rez << data.mid(lastPos, end - lastPos);
    return rez;
}

QList<QByteArray> smartSplit(
    const QByteArray& data,
    const char delimiter)
{
    return smartSplitInternal(data, delimiter, '\"', true);
}

QStringList smartSplit(
    const QString& data,
    const QChar delimiter,
    Qt::SplitBehavior splitBehavior)
{
    return smartSplitInternal(
        data,
        delimiter,
        QChar(L'\"'),
        splitBehavior == Qt::KeepEmptyParts);
}

QByteArray trimAndUnquote(const QByteArray& v)
{
    return trimAndUnquote(v, '\"');
}

QString trimAndUnquote(const QString& v)
{
    return trimAndUnquote(v, '\"');
}

static QByteArray doFormatJsonString(std::string_view data)
{
    int indent = 0;
    bool quoted = false;
    bool escaped = false;
    bool indentExpected = false;
    QByteArray result;
    result.reserve(data.size() * 2);

    for (char c: data)
    {
        if (c == '"' && !escaped)
            quoted = !quoted;

        escaped = c == '\\' && !escaped;

        if (indentExpected)
        {
            if (isSpaceOrControlChar(c))
                continue;

            indentExpected = false;
            if (c != ']' && c != '}')
            {
                result.append('\n');
                indent += 4;
                result.append(indent, ' ');
            }
            else
            {
                result.append(c);
                continue;
            }
        }

        if (quoted)
        {
            result.append(c);
        }
        else
        {
            if (isSpaceOrControlChar(c))
                continue;

            switch (c)
            {
                case ':':
                    result.append(c);
                    result.append(' ');
                    break;
                case '{':
                case '[':
                    result.append(c);
                    indentExpected = true;
                    break;
                case '}':
                case ']':
                    result.append('\n');
                    indent -= 4;
                    result.append(indent, ' ');
                    result.append(c);
                    break;
                case ',':
                    result.append(c);
                    result.append('\n');
                    result.append(indent, ' ');
                    break;
                default:
                    result.append(c);
                    break;
            }
        }
    }
    return result;
}

QByteArray formatJsonString(const QByteArray& data)
{
    return doFormatJsonString(std::string_view(data.data(), data.size()));
}

nx::Buffer formatJsonString(const nx::Buffer& data)
{
    return nx::Buffer(doFormatJsonString(std::string_view(data.data(), data.size())));
}

int stricmp(const QByteArray& left, const QByteArray& right)
{
    return stricmp(
        std::string_view(left.data(), left.size()),
        std::string_view(right.data(), right.size()));
}

void truncateToNul(QString* s)
{
    const int length = std::char_traits<char16_t>::length((const char16_t*) s->data());
    s->resize(length);
}

std::string toHex(const void* buffer, const int size, const std::string& delimeter)
{
    const uint8_t* data = (const uint8_t*)buffer;
    std::stringstream stream;
    stream << std::hex;
    for (int i = 0; i < size; ++i)
    {
        stream << std::setfill('0') << std::setw(2) << (int)data[i];
        if (i + 1 < size)
            stream << delimeter;
    }
    return stream.str();
}

NX_UTILS_API std::string half(const std::string& str)
{
    return str.substr(0, str.size() / 2) + "...";
}

QString normalizeSpaces(const QString& input)
{
    if (input.isEmpty())
        return input;

    QString result;
    result.reserve(input.length());
    bool inQuotes = false;
    bool lastWasSpace = true;

    for (const auto& c: input)
    {
        if (c == '"')
        {
            inQuotes = !inQuotes;
            result += c;
            lastWasSpace = false;
            continue;
        }

        if (inQuotes || !c.isSpace())
        {
            result += c;
            lastWasSpace = false;
            continue;
        }

        if (!lastWasSpace)
            result += ' ';
        lastWasSpace = true;
    }

    return lastWasSpace ? result.left(result.length() - 1) : result;
}

QStringList quoteDelimitedTokenList(const QString& input, const QStringList& delimiters)
{
    const auto processToken = [](const QString& token, int quoteCount)
    {
        if (token.isEmpty())
            return QString();

        auto trimmed = normalizeSpaces(token.trimmed());
        if ((trimmed.contains(' ') && !(trimmed.startsWith('"') && trimmed.endsWith('"')))
            || quoteCount > 2)
        {
            trimmed = '"' + trimmed + '"';
        }

        return trimmed;
    };

    QStringList result;
    QString currentToken;
    int quoteCount = 0;

    for (int i = 0; i < input.length(); ++i)
    {
        const auto& c = input[i];

        if (c == '"')
        {
            quoteCount++;
            currentToken += c;
            continue;
        }

        if (quoteCount % 2 == 0)
        {
            bool isDelimiter = false;
            for (const auto& delim: delimiters)
            {
                if (input.mid(i).startsWith(delim))
                {
                    if (!currentToken.isEmpty())
                        result << processToken(currentToken, quoteCount);
                    result << delim;
                    currentToken.clear();
                    i += delim.length() - 1;
                    quoteCount = 0;
                    isDelimiter = true;
                    break;
                }
            }
            if (isDelimiter)
                continue;
        }

        if (!currentToken.isEmpty() || !c.isSpace())
            currentToken += c;
    }

    if (!currentToken.isEmpty())
        result << processToken(currentToken, quoteCount);

    return result;
}

QString quoteDelimitedTokens(const QString& input, const QStringList& delimiters)
{
    if (input.isEmpty())
        return input;
    return quoteDelimitedTokenList(input, delimiters).join(" ");
}

QCollator createCollator(Qt::CaseSensitivity caseSensitivity, bool numericMode, QLocale locale)
{
    QCollator collator(locale);
    collator.setCaseSensitivity(caseSensitivity);
    collator.setNumericMode(numericMode);
    return collator;
}

} // namespace nx::utils
