#pragma once

#include <QtCore/QString>

namespace nx {
namespace utils {

/** Strength of a password. */
enum class PasswordStrength
{
    Good,     /**< password is good */
    Fair,     /**< password is satisfactory */
    Common,   /**< password is too commonly used */
    Weak,     /**< password contains too few character categories */
    Short,    /**< password contains too few characters */
    Incorrect /**< password contains invalid characters */
};

/** Function to analyze password strength. */
NX_UTILS_API PasswordStrength passwordStrength(const QString& password);
NX_UTILS_API PasswordStrength cameraPasswordStrength(const QString& password);

} // namespace utils
} // namespace nx
