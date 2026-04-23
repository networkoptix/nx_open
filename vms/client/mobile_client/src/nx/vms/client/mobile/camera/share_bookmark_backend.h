// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/mobile/timeline/abstract_object_data.h>

namespace nx::vms::client::mobile {

class ShareBookmarkBackend: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(timeline::AbstractObjectData* objectData
        READ objectData
        WRITE setObjectData
        NOTIFY objectDataChanged)

    Q_PROPERTY(bool isAvailable
        READ isAvailable
        NOTIFY isAvailableChanged)

    Q_PROPERTY(bool isShared
        READ isShared
        NOTIFY bookmarkChanged)

    Q_PROPERTY(bool isExpired //< Is true if shared and expired, otherwise false.
        READ isExpired
        NOTIFY bookmarkChanged)

    Q_PROPERTY(QString bookmarkName
        READ bookmarkName
        WRITE setBookmarkName
        NOTIFY bookmarkChanged)

    Q_PROPERTY(QString bookmarkDescription
        READ bookmarkDescription
        WRITE setBookmarkDescription
        NOTIFY bookmarkChanged)

    Q_PROPERTY(QString bookmarkDigest
        READ bookmarkDigest
        NOTIFY bookmarkChanged)

    Q_PROPERTY(qint64 expirationTimeMs
        READ expirationTimeMs
        NOTIFY bookmarkChanged)

public:
    enum class ExpiresIn
    {
        Hour,
        Day,
        Month,
        Never,
        Count
    };
    Q_ENUM(ExpiresIn)

    static void registerQmlType();

    ShareBookmarkBackend(QObject* parent = nullptr);
    virtual ~ShareBookmarkBackend() override;

    timeline::AbstractObjectData* objectData() const;
    void setObjectData(timeline::AbstractObjectData* value);

    bool isAvailable();

    bool isShared() const;
    bool isExpired() const;
    QString bookmarkName() const;
    void setBookmarkName(const QString& value);

    QString bookmarkDescription() const;
    void setBookmarkDescription(const QString& value);

    QString bookmarkDigest() const;

    qint64 expirationTimeMs() const;

    Q_INVOKABLE bool isNeverExpires() const;
    Q_INVOKABLE QString expiresInText() const;

    /**
     * Shares bookmark with specified parameters.
     * @param expirationTime Either ExpiresIn enum constant or actual "expires in" value.
     * @param password
     */
    Q_INVOKABLE bool share(qint64 expirationTime,
        const QString& password,
        bool showNativeShareSheet);
    Q_INVOKABLE bool stopSharing();
    Q_INVOKABLE void resetBookmarkData();

signals:
    void objectDataChanged();
    void bookmarkCreated();
    void bookmarkChanged();
    void isAvailableChanged();
    void sharingFailed();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
