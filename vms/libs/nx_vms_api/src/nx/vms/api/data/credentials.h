// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/vms/api/data/data_macros.h>

namespace nx::utils { class Url; }

namespace nx::vms::api {

struct NX_VMS_API Credentials
{
    /**%apidoc
     * %example admin
     */
    QString user;

    /**%apidoc
     * %example password123
     */
    QString password;

private:
    Q_GADGET
    Q_PROPERTY(QString user MEMBER user CONSTANT)
    Q_PROPERTY(QString password MEMBER password CONSTANT)

public:
    Credentials() = default;
    Credentials(const QString& user, const QString& password);
    Credentials(const nx::utils::Url& url);

    bool operator==(const Credentials& other) const = default;

    /**
     * Splits `value` by the first colon in `value` into user and password to create credentials
     * from. The colon and password after the colon are optional. I.e. `value` format is
     * `<user>[:<password>]`.
     */
    static Credentials parseColon(const QString& value, bool hidePassword = true);

    /**
     * Checks both user and password are empty.
     */
    Q_INVOKABLE bool isEmpty() const;

    /**
     * Checks both user and password are filled.
     */
    Q_INVOKABLE bool isValid() const;

    QString asString() const;
};
#define Credentials_Fields (user)(password)
NX_VMS_API_DECLARE_STRUCT_EX(Credentials, (csv_record)(json)(ubjson)(xml))
NX_REFLECTION_INSTRUMENT(Credentials, Credentials_Fields);

} // namespace nx::vms::api
