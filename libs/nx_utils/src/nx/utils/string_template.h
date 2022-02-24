// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
