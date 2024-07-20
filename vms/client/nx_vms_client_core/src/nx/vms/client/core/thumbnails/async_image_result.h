// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtGui/QImage>

namespace nx::vms::client::core {

/**
 * A notifier interface of when an asynchronously requested image is ready.
 * It's intended to be used in the GUI thread and is NOT thread-safe.
 * Descendants should call setImage when an image is obtained.
 */
class NX_VMS_CLIENT_CORE_API AsyncImageResult: public QObject
{
    Q_OBJECT

public:
    explicit AsyncImageResult(QObject* parent = nullptr);
    virtual ~AsyncImageResult() override = default;

    AsyncImageResult(const AsyncImageResult&) = delete;
    AsyncImageResult(AsyncImageResult&&) = delete;
    AsyncImageResult& operator=(const AsyncImageResult&) = delete;
    AsyncImageResult& operator=(AsyncImageResult&&) = delete;

    QImage image() const { return m_image; }

    QString errorString() const { return m_errorString; }

    /**
     * Whether the image is ready and can be queried and used.
     * May be set right after construction if the image was obtained synchronously.
     */
    bool isReady() const { return m_ready; }

    /** A key for image timestamp (in microseconds), if available, to be stored in QImage::text. */
    static const QString kTimestampUsKey;

    static std::chrono::microseconds timestamp(const QImage& image);
    static void setTimestamp(QImage& image, std::chrono::microseconds value);

signals:
    /**
     * Emitted when isReady becomes true.
     */
    void ready(QPrivateSignal);

protected:
    /**
     * Descendants must call this method when an image is asynchronously obtained.
     */
    virtual void setImage(const QImage& image);

    /**
    * Descendants must call this method when an error is encountered.
    * setImage({}) is automatically called from this function.
    */
    void setError(const QString& errorString);

private:
    bool m_ready = false;
    QImage m_image;
    QString m_errorString;
};

} // namespace nx::vms::client::core
