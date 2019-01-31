#include "placeholder_binder.h"

#include <vector>

#include <QtCore/QRegExp>
#include <QtCore/QStringRef>

namespace nx::vms::server::sdk_support {

namespace {

static const QRegExp kPlaceholderRegExp("\\{\\:[a-zA-z0-9]+\\}");

QString bindInternal(const QString& stringTemplate, const std::map<QString, QString> bindings)
{
    std::vector<QStringRef> placeholderRefs;
    int currentPosition = 0;
    while ((currentPosition = kPlaceholderRegExp.indexIn(stringTemplate, currentPosition)) != -1)
    {
        placeholderRefs.emplace_back(
            &stringTemplate,
            currentPosition,
            kPlaceholderRegExp.matchedLength());

        currentPosition += kPlaceholderRegExp.matchedLength();
    }

    if (placeholderRefs.empty())
        return stringTemplate;

    QString result = stringTemplate.left(placeholderRefs.cbegin()->position());
    for (std::size_t i = 0; i < placeholderRefs.size(); ++i)
    {
        const auto& ref = placeholderRefs[i];
        const auto bindingName = ref.mid(2, ref.size() - 3);
        const auto itr = bindings.find(bindingName.toString());

        if (itr != bindings.cend())
            result += itr->second;

        const auto currentRefEnd = ref.position() + ref.length();
        const auto lengthTillNextRef = i < placeholderRefs.size() - 1
            ? placeholderRefs[i + 1].position() - currentRefEnd
            : stringTemplate.size() - currentRefEnd;

        result += QStringRef(&stringTemplate, currentRefEnd, lengthTillNextRef);
    }

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

} // namespace nx::vms::server::sdk_support
