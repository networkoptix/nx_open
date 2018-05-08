#pragma once

#include <QtCore/QString>

namespace nx {
namespace utils {

struct NX_UTILS_API PasswordLimitations
{
    /** Minimal length of a password. */
    static constexpr int kMinimumLength = 8;

    /** Maximal length of a camera password. */
    static constexpr int kMaximumLengthForCamera = 15;

    /** Maximal length of a consecutive characters sequence. */
    static constexpr int kConsecutiveCharactersLimit = 4;

    /** Maximal number of repating characters/ */
    static constexpr int kRepeatingCharactersLimit = 4;

    /* Minimal acceptable number of character categories: */
    static constexpr int kMinimumCategories = 2;

    /* Preferred number of character categories: */
    static constexpr int kPreferredCategories = 3;

    static const QByteArray kAllowedSymbols;

    static const QByteArray kCameraAllowedSymbols;
};

/** Strength of a password. */
enum class PasswordStrength
{
    Good,           /**< Good. */
    Fair,           /**< Satisfactory. */
    Common,         /**< Too commonly used. */
    Weak,           /**< Contains too few character categories. */
    Short,          /**< Contains too few characters. */
    Conseq,         /**< Contains consecutive sequence of characters. */
    Repeat,         /**< Contains repeating characters. */
    Long,           /**< Contains too much characters. */
    Incorrect,      /**< Contains invalid characters. */
    IncorrectCamera,/**< Contains invalid characters for camera. */
    WeakAndFair,    /**< Weak, and still quite short. Actual for cameras. */
    WeakAndGood,    /**< Weak, has good length. Actual for cameras. */
};

NX_UTILS_API QString toString(PasswordStrength strength);

/** Function to analyze password strength. */
NX_UTILS_API PasswordStrength passwordStrength(const QString& password);
NX_UTILS_API PasswordStrength cameraPasswordStrength(const QString& password);

enum PasswordAcceptance
{
    Inacceptable = -1,
    Acceptable,
    Good
};

NX_UTILS_API PasswordAcceptance passwordAcceptance(PasswordStrength strength);

} // namespace utils
} // namespace nx
