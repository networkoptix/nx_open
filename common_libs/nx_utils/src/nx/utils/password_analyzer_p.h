#pragma once

/**
 * This is a private header intended to be included only to password_analyzer.cpp
 */

#include <array>
#include <nx/utils/log/assert.h>

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QTextStream>

namespace {

class CharCategoryLookup
{
public:
    enum Category
    {
        Invalid = -1,

        Number,
        LowercaseLetter,
        UppercaseLetter,
        Symbol,
        Space,

        ValidCategoryCount
    };

    CharCategoryLookup(const QByteArray& specialSymbols);

    Category operator[] (short index) const;

private:
    std::array<Category, 256> m_table;
};

class CommonPasswordsDictionary
{
public:
    CommonPasswordsDictionary(const QString& dictionaryPath = lit(":/common_passwords.txt"));

    bool operator () (const QString& password) const;

private:
    QSet<QString> m_dictionary;
};

//-------------------------------------------------------------------------------------------------

CharCategoryLookup::CharCategoryLookup(const QByteArray& specialSymbols)
{
    m_table.fill(Invalid);

    for (char ch = 'a'; ch <= 'z'; ++ch)
        m_table[ch] = LowercaseLetter;

    for (char ch = 'A'; ch <= 'Z'; ++ch)
        m_table[ch] = UppercaseLetter;

    for (char ch = '0'; ch <= '9'; ++ch)
        m_table[ch] = Number;

    m_table[' '] = Space;

    for (char ch : specialSymbols)
    {
        m_table[static_cast<unsigned char>(ch)] = Symbol;
    }
}

CharCategoryLookup::Category CharCategoryLookup::operator[] (short index) const
{
    return (index >= 0 && (size_t)index < m_table.size()) ? m_table[index] : Invalid;
}

CommonPasswordsDictionary::CommonPasswordsDictionary(const QString& dictionaryPath)
{
    QFile passwords(dictionaryPath);
    if (!passwords.open(QFile::ReadOnly))
    {
        NX_ASSERT(false);
        return;
    }

    QTextStream reader(&passwords);

    while (!reader.atEnd())
        m_dictionary.insert(reader.readLine().trimmed());
}

bool CommonPasswordsDictionary::operator () (const QString& password) const
{
    return m_dictionary.contains(password);
}

} // namespace
