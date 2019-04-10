#pragma once

#include <map>
#include <optional>

#include <QtCore/QString>

namespace nx::utils {

// TODO: #dmishin currently there is no way to escape placeholders.
class NX_UTILS_API PlaceholderBinder
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

} // namespace nx::utils
