#pragma once

#include <string>
#include <vector>

namespace nx::utils::algorithm {

/**
 * Implements Knuth-Morris-Pratt substring search algorithm.
 */
class NX_UTILS_API KmpSearcher
{
public:
    KmpSearcher(std::string str);

    const std::string& str() const;

    /**
     * @return Distance from the end of the last processed character to the occurence found.
     * std::string::npos if nothing found.
     * NOTE: Processing is stopped with any occurence. So, KmpSearcher::process has to be invoked 
     * again with unprocessed text (textPart.substr(textPart.size() + str.size() - result)).
     */
    std::string::size_type process(const std::string_view& textPart);

    /**
     * @return E.g., if string to find is "bcd" and text "198nrabc" has been provided, then 2 is 
     * returned. It means that string can potentially be found 2 positions prior to the last position.
     */
    std::size_t potentialOccurenceLag() const;

private:
    const std::string m_str;
    const std::vector<int> m_prefixTable;
    std::size_t m_matchedPrefixLength = 0;

    std::vector<int> computePrefixTable(const std::string_view& str);
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API KmpReplacer
{
public:
    KmpReplacer(
        std::string before,
        std::string after);

    /**
     * To dump the internal buffer invoke KmpReplacer::process with an empty string.
     */
    std::string process(std::string textPart);

private:
    KmpSearcher m_searcher;
    const std::string m_after;
    std::string m_cache;
};

//-------------------------------------------------------------------------------------------------

NX_UTILS_API std::string::size_type kmpFindNext(
    const std::string_view& text,
    const std::string_view& str,
    std::string::size_type pos = 0);

NX_UTILS_API std::vector<std::string::size_type> kmpFindAll(
    const std::string_view& text,
    const std::string_view& str);

NX_UTILS_API void kmpReplace(
    std::string* text,
    const std::string& before,
    const std::string& after);

NX_UTILS_API std::string kmpReplaced(
    std::string text,
    const std::string& before,
    const std::string& after);

} // namespace nx::utils::algorithm
