#pragma once

#include <QUrlQuery>
#include <QVariant>

namespace nx::utils {

namespace detail {

bool NX_UTILS_API convert(const QString& value, QString* outObject);
bool NX_UTILS_API convert(const QString& value, std::string* outObject);
bool NX_UTILS_API convert(const QString& value, int* outObject);

template<typename T>
bool convert(const QString& /*value*/, T* /*outObject*/)
{
    return false;
}

} // namespace detail

class NX_UTILS_API UrlQuery
{
public:
    UrlQuery() = default;
    UrlQuery(const QString& query);

    UrlQuery& add(const QString& key, const QString& value);
    UrlQuery& add(const QString& key, const std::string& value);
    UrlQuery& add(const char* key, const char* value);

    template<
        typename NumericType,
        typename = typename std::enable_if_t<std::is_arithmetic_v<NumericType>, NumericType>>
    UrlQuery& add(const QString& key, NumericType value)
    {
        return add(key, QString::number(value));
    }

    bool hasKey(const QString& key) const;

    /**
     * @return true if the value converted to the desired type successfully
     * NOTE: requires template specialization for new types, see detail namespace.
     */
    template<typename ObjectType>
    bool value(
        const QString& key,
        ObjectType* outObject,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const
    {
        return detail::convert(m_query.queryItemValue(key, encoding), outObject);
    }

    /**
     * @return true if the key exists and the value converted to the desired type successfully
     * NOTE: requires template specialization for new types, see detail namespace.
     */
    template<typename ObjectType>
    bool valueIfExists(
        const QString& key,
        ObjectType* outObject,
        QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const
    {
        return hasKey(key) && value(key, outObject, encoding);
    }

    QString toString(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;

    operator const QUrlQuery&() const;

private:
    QUrlQuery m_query;
};

} // namespace nx::utils
