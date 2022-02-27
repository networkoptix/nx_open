// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace nx::vms::client::desktop {

/**
 * Session id, which will be used as a folder name to keep session state. Case-insensitive, stable,
 * uses only filesystem-allowed symbols.
 */
struct NX_VMS_CLIENT_DESKTOP_API SessionId
{
    /** Byte size of the serialized session id representation. */
    static constexpr int kDataSize = 32;

    SessionId();
    SessionId(const QString& systemId, const QString& userId);

    static SessionId deserialized(const QByteArray& serializedValue);
    QByteArray serialized() const;

    operator QString() const;
    QString toString() const;

    static SessionId fromString(const QString& value);

    bool operator!=(const SessionId& other) const;
    bool operator==(const SessionId& other) const;

private:
    QByteArray m_bytes;
};

/** GTest output support. */
NX_VMS_CLIENT_DESKTOP_API void PrintTo(const SessionId& value, ::std::ostream* os);

} // namespace nx::vms::client::desktop
