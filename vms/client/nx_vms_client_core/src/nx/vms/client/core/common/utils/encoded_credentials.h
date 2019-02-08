#pragma once

#include <QtCore/QtGlobal>

#include <nx/vms/client/core/common/utils/encoded_string.h>
#include <nx/utils/url.h>

namespace nx::vms::client::core {

struct EncodedCredentials
{
    QString user;
    EncodedString password;

private:
    Q_GADGET
    Q_PROPERTY(QString user MEMBER user CONSTANT)
    Q_PROPERTY(QString password READ decodedPassword CONSTANT)

public:
    EncodedCredentials() = default;
    EncodedCredentials(
        const QString& user,
        const QString& password,
        const QByteArray& key = QByteArray());

    explicit EncodedCredentials(const nx::utils::Url& url, const QByteArray& key = QByteArray());

    /** Extra cryptography key. */
    QByteArray key() const;

    /** Reset extra cryptography key. All calculated and cached values will be invalidated. */
    void setKey(const QByteArray& key);

    QString decodedPassword() const;

    Q_INVOKABLE bool isEmpty() const;

    //! Check both user and password are filled.
    Q_INVOKABLE bool isValid() const;
};

#define EncodedCredentials_Fields (user)(password)

QN_FUSION_DECLARE_FUNCTIONS(EncodedCredentials, (eq)(json))

void PrintTo(const EncodedCredentials& value, ::std::ostream* os);

} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::EncodedCredentials)
