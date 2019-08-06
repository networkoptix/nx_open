#include <gtest/gtest.h>

#include <nx/utils/algorithm/kmp.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/random.h>

namespace nx::utils::algorithm::test {

namespace {

static std::string generateRandomString(
    const char* alphabet,
    std::size_t length)
{
    const auto alphabetLength = strlen(alphabet);
    std::string result(length, 0);
    for (auto& ch: result)
        ch = alphabet[rand() % alphabetLength];
    return result;
}

static std::string replaceAll(
    const std::string& text,
    const std::string& before,
    const std::string& after)
{
    std::string result;
    for (std::size_t pos = 0; pos < text.size();)
    {
        const auto oldPos = pos;
        pos = text.find(before, pos);
        if (pos == std::string::npos)
        {
            result += text.substr(oldPos);
            break;
        }

        result += text.substr(oldPos, pos - oldPos);
        result += after;
        pos += before.size();
    }

    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

TEST(Kmp, search)
{
    ASSERT_EQ(0, kmpFindNext("ffa", "ffa"));
    ASSERT_EQ(1, kmpFindNext("fffa3dc", "ffa"));
    ASSERT_EQ(0, kmpFindNext("ffa3dc", "ffa"));
    ASSERT_EQ(6, kmpFindNext("1fa3dcffa", "ffa"));
    ASSERT_EQ(6, kmpFindNext("ffa3dcffa", "ffa", 1));
    
    ASSERT_EQ(std::string::npos, kmpFindNext("c43ebddnladfbfdlflcndnn4e", "4f"));
    ASSERT_EQ(std::string::npos, kmpFindNext("", "4f"));
    ASSERT_EQ(std::string::npos, kmpFindNext("4", "4f"));
}

TEST(Kmp, replace)
{
    ASSERT_EQ("--bc123", kmpReplaced("abcbc123", "abc", "--"));
    ASSERT_EQ("--bc123--", kmpReplaced("abcbc123abc", "abc", "--"));
    ASSERT_EQ("--", kmpReplaced("abc", "abc", "--"));
    ASSERT_EQ("--dabaa----adpem--", kmpReplaced("abcdabaaabcabcadpemabc", "abc", "--"));
    ASSERT_EQ("111111", kmpReplaced("222222", "2", "1"));

    ASSERT_EQ("abd", kmpReplaced("abd", "abc", "--"));
    ASSERT_EQ("1", kmpReplaced("1", "abc", "--"));
    ASSERT_EQ("", kmpReplaced("", "abc", "--"));
    ASSERT_EQ("1232454567", kmpReplaced("1232454567", "abc", "--"));
}

TEST(Kmp, replace_stress_test)
{
    constexpr char alphabet[] = "abcdlenf34";
    constexpr auto textLength = 250;
    constexpr auto maxStrLength = 11;
    constexpr auto loopSize = 99;

    for (int i = 0; i < loopSize; ++i)
    {
        auto text = generateRandomString(alphabet, textLength);
        const auto before = generateRandomString(alphabet, (rand() % maxStrLength) + 1);
        const std::string after = generateRandomString(alphabet, (rand() % maxStrLength) + 1);

        ASSERT_EQ(
            replaceAll(text, before, after),
            kmpReplaced(text, before, after));
    }
}

} // namespace nx::utils::algorithm::test
