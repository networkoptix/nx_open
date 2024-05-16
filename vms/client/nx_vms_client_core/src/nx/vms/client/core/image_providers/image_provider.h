// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>

#include <nx/vms/client/core/client_core_globals.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ImageProvider: public QObject
{
    Q_OBJECT

public:
    ImageProvider(QObject* parent = nullptr);
    virtual ~ImageProvider() override = default;

    virtual QImage image() const = 0;
    virtual QSize sizeHint() const = 0;
    virtual ThumbnailStatus status() const = 0;

    virtual bool tryLoad(); //< Returns false if slow asynchronous load is required.

public slots:
    void loadAsync();

signals:
    void imageChanged(const QImage& image);
    void statusChanged(ThumbnailStatus status);
    void sizeHintChanged(const QSize& sizeHint);

protected:
    virtual void doLoadAsync() = 0;
};

class NX_VMS_CLIENT_CORE_API BasicImageProvider: public ImageProvider
{
    Q_OBJECT

public:
    BasicImageProvider(const QImage& image, QObject* parent = nullptr);
    virtual ~BasicImageProvider() override = default;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual ThumbnailStatus status() const override;

    virtual bool tryLoad() override;

protected:
    virtual void doLoadAsync() override;

private:
    QImage m_image;
};

} // namespace nx::vms::client::core
