#include "password_analyzer.h"
#include "password_analyzer_p.h"

#include <numeric>
#include <string>
#include <array>

#ifdef _DEBUG
//#define ALLOW_ANY_PASSWORD
#endif

namespace {

/* Minimal acceptable number of character categories: */
const int kMinimumCategories = 2;

/* Preferred number of character categories: */
const int kPreferredCategories = 3;


int differentSign(int left, int right)
{
    static const auto sign =
        [](int value){ return (value > 0) - (value < 0); };

    return sign(left) != sign(right);
}

bool hasRepeatingSymbols(const QString& password,
    int maxCount = nx::utils::PasswordLimitations::kRepeatingCharactersLimit)
{
    const auto count = password.count();
    if (!maxCount || count < maxCount || count <= 1)
        return false;

    const auto maxPairsCount = maxCount - 1;

    int pairsCount = 0;
    char prev = password.begin()->toLatin1();
    for (auto it = password.begin() + 1; it != password.end(); ++it)
    {
        const char current = it->toLatin1();
        if (prev != current)
            pairsCount = 0;
        else if (++pairsCount >= maxPairsCount)
            return true;

        prev = current;
    }

    return false;
}

bool hasConsecutiveSequence(const QString& password,
    int maxCount = nx::utils::PasswordLimitations::kConsecutiveCharactersLimit)
{
    const auto count = password.count();
    if (!maxCount || count < maxCount || count <= 1)
        return false;

    const auto maxSequentialPairsCount = maxCount - 1;

    int sequentialPairsCount = 0;
    char prev = password.begin()->toLatin1();
    for (auto it = password.begin() + 1; it != password.end(); ++it)
    {
        const auto current = it->toLatin1();
        const auto diff = prev - current;
        if (std::abs(diff) != 1 || diff == 0) // Non sequential symbols
            sequentialPairsCount = 0;
        else if (differentSign(diff, sequentialPairsCount)) // Sequence changed direction
            sequentialPairsCount = diff;
        else if (std::abs(sequentialPairsCount += diff) >= maxSequentialPairsCount)
            return true;

        prev = current;
    }

    return false;
}

} // namespace

namespace nx {
namespace utils {

const QByteArray PasswordLimitations::kAllowedSymbols = "~!@#$%^&*()-=_+[]{};:,.<>?`'\"|/\\";
const QByteArray PasswordLimitations::kCameraAllowedSymbols = "~`!@#$%^*()_-+=|{}[].?/";

PasswordStrength passwordStrength(const QString& password)
{
#ifdef ALLOW_ANY_PASSWORD
    return PasswordStrength::Good;
#endif
    /* Whether a space is allowed (in the middle): */
    static constexpr bool kAllowSpace = true;

    /* Allowed non-unicode special characters for a password: */

    std::array<int, CharCategoryLookup::ValidCategoryCount> categories;
    categories.fill(0);

    for (QString::ConstIterator ch = password.cbegin(); ch != password.cend(); ++ch)
    {
        static const CharCategoryLookup categoryLookup(PasswordLimitations::kAllowedSymbols);
        CharCategoryLookup::Category category = categoryLookup[ch->unicode()];

        if (category == CharCategoryLookup::Space)
        {
            category = (kAllowSpace && ch != password.cbegin() && ch != password.cend() - 1) ?
                CharCategoryLookup::Symbol:
                CharCategoryLookup::Invalid;
        }

        if (category == CharCategoryLookup::Invalid)
            return PasswordStrength::Incorrect;

        categories[category] = 1;
    }

    if (password.length() < PasswordLimitations::kMinimumLength)
        return PasswordStrength::Short;

    static const CommonPasswordsDictionary commonPasswords;
    if (commonPasswords(password))
        return PasswordStrength::Common;

    int numCategories = std::accumulate(categories.cbegin(), categories.cend(), 0);
    if (numCategories < kMinimumCategories)
        return PasswordStrength::Weak;

    if (numCategories < kPreferredCategories)
        return PasswordStrength::Fair;

    return PasswordStrength::Good;
}

PasswordStrength cameraPasswordStrength(const QString& password)
{
    std::array<int, CharCategoryLookup::ValidCategoryCount> categories;
    categories.fill(0);

    for (QString::ConstIterator ch = password.cbegin(); ch != password.cend(); ++ch)
    {
        static const CharCategoryLookup categoryLookup(PasswordLimitations::kCameraAllowedSymbols);
        auto category = categoryLookup[ch->unicode()];

        if (category == CharCategoryLookup::Space
            && ch != password.cbegin()
            && ch != password.cend() - 1)
        {
            category = CharCategoryLookup::Symbol;
        }

        if (category == CharCategoryLookup::Invalid || category == CharCategoryLookup::Space)
            return PasswordStrength::IncorrectCamera;

        categories[category] = 1;
    }

    const auto length = password.length();
    if (length < PasswordLimitations::kMinimumLength)
        return PasswordStrength::Short;

    if (length > PasswordLimitations::kMaximumLengthForCamera)
        return PasswordStrength::Long;

    static constexpr int kFairPasswordMaxLength = 9;

    const auto numCategories = std::accumulate(categories.cbegin(), categories.cend(), 0);

    const auto fairPasswordCategory = length <= kFairPasswordMaxLength;
    if (fairPasswordCategory)
    {
        if (numCategories < kPreferredCategories)
            return PasswordStrength::Weak;
    }
    else if (numCategories < kMinimumCategories)
    {
        return PasswordStrength::Weak;
    }

    if (hasConsecutiveSequence(password))
        return PasswordStrength::Conseq;

    if (hasRepeatingSymbols(password))
        return PasswordStrength::Repeat;

    return fairPasswordCategory ? PasswordStrength::Fair : PasswordStrength::Good;
}

} // namespace utils
} // namespace nx
