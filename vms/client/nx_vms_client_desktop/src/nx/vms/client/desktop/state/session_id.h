// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

/**
 * Session ID, which will be used as a folder name to keep session state. Case-insensitive, stable,
 * uses only filesystem-allowed symbols.
 */
struct NX_VMS_CLIENT_DESKTOP_API SessionId
{
    /** Byte size of the serialized session ID representation. */
    static constexpr int kDataSize = 32;

    SessionId();

    /**
     * Constructs SessionId for the given systemId and userId.
     * localSystemId is not used in the session ID calculation. It is used to get the legacy
     * session ID which used local IDs only.
     */
    SessionId(const nx::Uuid& localSystemId, const QString& systemId, const QString& userId);

    bool isEmpty() const;
    operator bool() const;

    static SessionId deserialized(const QByteArray& serializedValue);
    QByteArray serialized() const;

    std::string toString() const;
    QString toQString() const;

    /**
     * Returns the string representation calculated from the user ID and always the
     * **local system ID**. New session IDs are calculated from cloud system ID when system is
     * linked to the cloud.
     */
    QString legacyIdString() const;

    static SessionId fromString(const std::string& value);

    QString toLogString() const;

    bool operator!=(const SessionId& other) const;
    bool operator==(const SessionId& other) const;

private:
    QByteArray m_bytes;
    nx::Uuid m_localSystemId;
    QString m_systemId;
    QString m_userId;
};

NX_REFLECTION_TAG_TYPE(SessionId, useStringConversionForSerialization)

/** GTest output support. */
NX_VMS_CLIENT_DESKTOP_API void PrintTo(const SessionId& value, ::std::ostream* os);

} // namespace nx::vms::client::desktop
