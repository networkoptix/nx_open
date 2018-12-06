#pragma once

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#include <nx/network/http/http_client.h>
#include <nx/utils/std/optional.h>

namespace nx {
namespace plugins {
namespace utils {

std::unique_ptr<nx::network::http::HttpClient> makeHttpClient(const QAuthenticator& authenticator);
bool isResponseOK(const nx::network::http::HttpClient* client);

class XmlRequestHelper
{
public:
    XmlRequestHelper(
        nx::utils::Url url,
        const QAuthenticator& authenticator,
        nx::network::http::AuthType authType = nx::network::http::AuthType::authDigest);

    class Result
    {
    public:
        Result(const XmlRequestHelper* parent, QDomElement element, QStringList path);
        QString path() const;

        std::optional<Result> child(const QString& name) const;
        std::vector<Result> children(const QString& name) const;

        std::optional<QString> string(const QString& name) const;
        std::optional<int> integer(const QString& name) const;
        std::optional<bool> boolean(const QString& name) const;
        std::optional<QString> attribute(const QString& name) const;

        QString stringOrEmpty(const QString& name) const;
        int integerOrZero(const QString& name) const;
        bool booleanOrFalse(const QString& name) const;
        QString attributeOrEmpty(const QString& name) const;

    private:
        const XmlRequestHelper* const m_parent;
        const QDomElement m_element;
        const QStringList m_path;
    };

    std::optional<Result> get(const QString& path);
    bool put(const QString& path, const QString& data = {});
    bool post(const QString& path, const QString& data = {});
    bool delete_(const QString& path);

    std::optional<QByteArray> readRawBody();
    std::optional<Result> readBody();

    QString idForToStringFromPtr() const;

protected:
    const nx::utils::Url m_url;
    std::unique_ptr<nx::network::http::HttpClient> m_client;
};

} // namespace utils
} // namespace plugins
} // namespace nx
