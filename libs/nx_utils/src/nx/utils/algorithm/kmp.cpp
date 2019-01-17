#include "kmp.h"

namespace nx::utils::algorithm {

KmpSearcher::KmpSearcher(std::string str):
    m_str(std::move(str)),
    m_prefixTable(computePrefixTable(m_str))
{
}

const std::string& KmpSearcher::str() const
{
    return m_str;
}

std::string::size_type KmpSearcher::process(const std::string_view& textPart)
{
    for (std::size_t pos = 0; pos < textPart.size();)
    {
        if (m_str[m_matchedPrefixLength] == textPart[pos])
        {
            ++pos;
            ++m_matchedPrefixLength;
            if (m_matchedPrefixLength == m_str.size())
            {
                m_matchedPrefixLength = 0;
                return textPart.size() - pos + m_str.size();
            }
        }
        else
        {
            if (m_prefixTable[m_matchedPrefixLength] < 0)
            {
                ++pos;
                m_matchedPrefixLength = 0;
            }
            else
            {
                m_matchedPrefixLength = m_prefixTable[m_matchedPrefixLength];
            }
        }
    }

    return std::string::npos;
}

std::size_t KmpSearcher::potentialOccurenceLag() const
{
    return m_matchedPrefixLength;
}

std::vector<int> KmpSearcher::computePrefixTable(const std::string_view& str)
{
    const auto len = str.length();
    std::vector<int> t(len + 1);
    t[0] = -1;

    int cnd = 0;
    for (int pos = 1; pos < len; ++pos)
    {
        if (str[pos] == str[cnd])
        {
            t[pos] = t[cnd];
        }
        else
        {
            t[pos] = cnd;
            cnd = t[cnd];
            while (cnd >= 0 && str[pos] != str[cnd])
                cnd = t[cnd];
        }
        ++cnd;
    }

    t[len] = cnd;

    return t;
}

//-------------------------------------------------------------------------------------------------

KmpReplacer::KmpReplacer(
    std::string before,
    std::string after)
    :
    m_searcher(std::move(before)),
    m_after(std::move(after))
{
}

std::string KmpReplacer::process(std::string textPart)
{
    if (textPart.empty())
        return std::exchange(m_cache, std::string());

    std::string textToReturn;
    while (!textPart.empty())
    {
        const auto posFound = m_searcher.process(textPart);

        if (posFound == std::string::npos && m_searcher.potentialOccurenceLag() == 0)
        {
            return std::move(textToReturn) +
                std::exchange(m_cache, std::string()) +
                std::move(textPart);
        }

        if (posFound == std::string::npos)
        {
            m_cache += std::move(textPart);
            const auto pos = m_cache.size() - m_searcher.potentialOccurenceLag();
            textToReturn += m_cache.substr(0, pos);
            m_cache = m_cache.substr(pos);
            return textToReturn;
        }

        textToReturn += std::exchange(m_cache, std::string());
        const auto pos = textPart.size() + m_searcher.str().size() - posFound;
        textToReturn += textPart.substr(0, pos);
        textPart = textPart.substr(pos);

        textToReturn.replace(
            textToReturn.size() - m_searcher.str().size(), m_searcher.str().size(),
            m_after);
    }

    return textToReturn;
}

//-------------------------------------------------------------------------------------------------

std::string::size_type kmpFindNext(
    const std::string_view& text,
    const std::string_view& str,
    std::string::size_type pos)
{
    KmpSearcher searcher(std::string(str.data(), str.size()));

    const auto result = searcher.process(text.substr(pos));
    if (result == std::string::npos)
        return result;
    return text.size() - result;
}

std::vector<std::string::size_type> kmpFindAll(
    const std::string_view& text,
    const std::string_view& before)
{
    std::vector<std::string::size_type> result;

    std::size_t pos = 0;
    for (;;)
    {
        pos = kmpFindNext(text, before, pos);
        if (pos == std::string::npos)
            break;

        result.push_back(pos);
        ++pos;
    }

    return result;
}

void kmpReplace(
    std::string* text,
    const std::string& before,
    const std::string& after)
{
    KmpReplacer replacer(before, after);
    *text = replacer.process(*text);
    *text += replacer.process(std::string());
}

std::string kmpReplaced(
    std::string text,
    const std::string& before,
    const std::string& after)
{
    kmpReplace(&text, before, after);
    return text;
}

} // namespace nx::utils::algorithm
