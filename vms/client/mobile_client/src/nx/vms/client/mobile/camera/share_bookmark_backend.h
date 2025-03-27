// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QModelIndex>
#include <QtCore/QUrl>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::mobile {

class ShareBookmarkBackend: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QModelIndex modelIndex
        READ modelIndex
        WRITE setModelIndex
        NOTIFY modelIndexChanged)

    Q_PROPERTY(bool isAvailable
        READ isAvailable
        NOTIFY isAvailableChanged)

    Q_PROPERTY(bool isShared
        READ isShared
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

    QModelIndex modelIndex() const;
    void setModelIndex(const QModelIndex& value);

    bool isAvailable();

    bool isShared() const;
    QString bookmarkName() const;
    void setBookmarkName(const QString& value);

    QString bookmarkDescription() const;
    void setBookmarkDescription(const QString& value);

    QString bookmarkDigest() const;

    qint64 expirationTimeMs() const;

    Q_INVOKABLE bool isSharedNow() const;
    Q_INVOKABLE bool isNeverExpires() const;
    Q_INVOKABLE QString expiresInText() const;

    /**
     * Shares bookmark with specified parameters.
     * @param expirationTime Either ExpiresIn enum constant or actual "expires in" value.
     * @param password
     */
    Q_INVOKABLE bool share(qint64 expirationTime, const QString& password);
    Q_INVOKABLE bool stopSharing();

signals:
    void modelIndexChanged();
    void bookmarkChanged();
    void isAvailableChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
