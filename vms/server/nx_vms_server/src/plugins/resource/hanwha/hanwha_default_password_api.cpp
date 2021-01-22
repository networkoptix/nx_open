#include "hanwha_default_password_api.h"

#include <plugins/resource/hanwha/hanwha_resource.h>

#include <nx/network/http/http_client.h>

namespace nx::vms::server::plugins {

HanwhaDefaultPasswordApi::HanwhaDefaultPasswordApi(HanwhaResource* device):
    m_device(device)
{
}

std::optional<HanwhaDefaultPasswordApi::Info> HanwhaDefaultPasswordApi::fetchInfo()
{
    Info result;

    nx::network::http::HttpClient httpClient;

    nx::utils::Url defaultPasswordInfoRequestUrl = m_device->getUrl();
    defaultPasswordInfoRequestUrl.setPath("/init-cgi/pw_init.cgi");

    QUrlQuery defaultPasswordRequestQuery;
    defaultPasswordRequestQuery.addQueryItem("msubmenu", "statuscheck");
    defaultPasswordRequestQuery.addQueryItem("action", "view");

    defaultPasswordInfoRequestUrl.setQuery(defaultPasswordRequestQuery);

    bool success = httpClient.doGet(defaultPasswordInfoRequestUrl);
    if (!success)
    {
        NX_DEBUG(this, "Unable to retrieve public key to cipher password, error: %1, device: %2",
            httpClient.lastSysErrorCode(), m_device);
        return std::nullopt;
    }

    const std::optional<nx::Buffer> response = httpClient.fetchEntireMessageBody();
    if (!response)
    {
        NX_DEBUG(this, "Public key response is unavailable, device: %1",
            m_device);
        return std::nullopt;
    }

    const auto startIndex = response->indexOf("-----BEGIN RSA PUBLIC KEY-----");
    if (startIndex < 0)
    {
        NX_DEBUG(this, "Unable to retrieve public key from response, device %1, response: %2",
            m_device, *response);
        return std::nullopt;
    }

    const QString kEndKeySignature = "-----END RSA PUBLIC KEY-----";

    const auto endIndex = response->indexOf(kEndKeySignature);

    if (endIndex < 0)
    {
        NX_DEBUG(this, "Unable to find the end key signature, device %1, response: %2",
            m_device, *response);
        return std::nullopt;
    }

    result.publicKey = response->mid(startIndex, endIndex - startIndex + kEndKeySignature.length());

    for (const auto& line: response->split('\n'))
    {
        const auto trimmedLine = line.trimmed();
        if (!trimmedLine.startsWith("Initialized="))
            continue;

        const auto nameAndValue = trimmedLine.split('=');
        if (nameAndValue.size() < 2)
            continue;

        result.isInitialized = fromHanwhaString<bool>(nameAndValue[1]);
    }

    return result;
}

bool HanwhaDefaultPasswordApi::setPassword(const QString& password)
{
    nx::network::http::HttpClient httpClient;

    const std::optional<Info> defaultPasswordInfo = fetchInfo();
    if (!defaultPasswordInfo)
    {
        NX_DEBUG(this, "Unable to fetch default password info, device %1", m_device);
        return false;
    }

    nx::Buffer encryptedPassword =
        encryptPasswordUrlEncoded(password.toUtf8(), defaultPasswordInfo->publicKey);

    nx::utils::Url passwordSetRequestUrl = m_device->getUrl();
    passwordSetRequestUrl.setPath("/init-cgi/pw_init.cgi");

    QUrlQuery passwordSetRequestQuery;
    passwordSetRequestQuery.addQueryItem("msubmenu", "setinitpassword");
    passwordSetRequestQuery.addQueryItem("action", "set");
    passwordSetRequestQuery.addQueryItem("IsPasswordEncrypted", "True");

    passwordSetRequestUrl.setQuery(passwordSetRequestQuery);

    return httpClient.doPost(passwordSetRequestUrl, "text/plain", encryptedPassword);
}

} // namespace nx::vms::server::plugins