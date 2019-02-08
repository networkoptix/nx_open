#pragma once

#include <map>

#include <QtCore/QString>

#include <nx/utils/std/optional.h>

namespace nx::vms::server::sdk_support {

// TODO: #dmishin currently there is no way to escape placeholders.
class PlaceholderBinder
{
public:
    PlaceholderBinder(QString stringTemplate);
    QString str() const;
    void bind(const std::map<QString, QString>& bindings);
    void clearBindings();

private:
    const QString m_stringTemplate;
    std::map<QString, QString> m_bindings;
    mutable std::optional<QString> m_cachedResult;
};

} // namespace nx::vms::server::sdk_support
