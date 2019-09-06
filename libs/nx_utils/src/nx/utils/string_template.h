#include <functional>
#include <QtCore/QString>

namespace nx::utils {

/**
 * Replaces every entry like "<variableMark>name" in templateString by calling resolve("name").
 */
NX_UTILS_API QString stringTemplate(
    const QString& template_,
    const QString& variableMark,
    const std::function<QString(const QString& name)> resolve);

} // namespace nx::utils
