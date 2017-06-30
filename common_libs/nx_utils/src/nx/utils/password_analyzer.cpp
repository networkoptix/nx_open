#include "password_analyzer.h"
#include "password_analyzer_p.h"

#include <numeric>
#include <string>
#include <array>

#ifdef _DEBUG
//#define ALLOW_ANY_PASSWORD
#endif

namespace {

/* Minimal length of a password: */
const int kMinimumLength = 8;

/* Minimal acceptable number of character categories: */
const int kMinimumCategories = 2;

/* Preferred number of character categories: */
const int kPreferredCategories = 3;

/* Whether a space is allowed (in the middle): */
const bool kAllowSpace = true;

/* Allowed non-unicode special characters for a password: */
const QByteArray kAllowedSymbols = "~!@#$%^&*()-=_+[]{};:,.<>?`'\"|/\\";

} // namespace

namespace nx {
namespace utils {

PasswordStrength passwordStrength(const QString& password)
{
#ifdef ALLOW_ANY_PASSWORD
    return PasswordStrength::Good;
#endif

    std::array<int, CharCategoryLookup::ValidCategoryCount> categories;
    categories.fill(0);

    for (QString::ConstIterator ch = password.cbegin(); ch != password.cend(); ++ch)
    {
        static const CharCategoryLookup categoryLookup(kAllowedSymbols);
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

    if (password.length() < kMinimumLength)
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

} // namespace utils
} // namespace nx
