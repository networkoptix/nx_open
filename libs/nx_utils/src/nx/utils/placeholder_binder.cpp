#include "placeholder_binder.h"

namespace nx::utils {

namespace {

QString bindInternal(const QString& stringTemplate, const std::map<QString, QString> bindings)
{
    QString result = stringTemplate;
    for (const auto& binding: bindings)
        result.replace(QString("{:") + binding.first + "}", binding.second);

    return result;
}

} // namespace

PlaceholderBinder::PlaceholderBinder(QString stringTemplate):
    m_stringTemplate(std::move(stringTemplate))
{
}

QString PlaceholderBinder::str() const
{
    if (!m_cachedResult)
        m_cachedResult = bindInternal(m_stringTemplate, m_bindings);

    return *m_cachedResult;
}

void PlaceholderBinder::bind(const std::map<QString, QString>& bindings)
{
    m_bindings.insert(bindings.cbegin(), bindings.cend());
    m_cachedResult = std::nullopt;
}

void PlaceholderBinder::clearBindings()
{
    m_bindings.clear();
    m_cachedResult = std::nullopt;
}

} // namespace nx::utils
