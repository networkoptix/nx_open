#include "update_utils.h"

QString passwordForBuild(const QString& build)
{
    static constexpr char kPasswordChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static constexpr int kPasswordLength = 6;

    // Leaving only numbers from the build string, as it may be a changeset.
    QString buildNum;
    for (const QChar& c: build)
    {
        if (c.isDigit())
            buildNum.append(c);
    }

    unsigned buildNumber = unsigned(buildNum.toInt());

    unsigned seed1 = buildNumber;
    unsigned seed2 = (buildNumber + 13) * 179;
    unsigned seed3 = buildNumber << 16;
    unsigned seed4 = ((buildNumber + 179) * 13) << 16;
    unsigned seed = seed1 ^ seed2 ^ seed3 ^ seed4;

    QString password;
    const int charsCount = sizeof(kPasswordChars) - 1;
    for (int i = 0; i < kPasswordLength; i++)
    {
        password += QChar::fromLatin1(kPasswordChars[seed % charsCount]);
        seed /= charsCount;
    }
    return password;
}
