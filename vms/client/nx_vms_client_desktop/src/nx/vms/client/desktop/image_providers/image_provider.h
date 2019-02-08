#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>

#include <client/client_globals_fwd.h>

namespace nx::vms::client::desktop {

class ImageProvider: public QObject
{
    Q_OBJECT

public:
    ImageProvider(QObject* parent = nullptr);
    virtual ~ImageProvider() override = default;

    virtual QImage image() const = 0;
    virtual QSize sizeHint() const = 0;
    virtual Qn::ThumbnailStatus status() const = 0;

public slots:
    void loadAsync();

signals:
    void imageChanged(const QImage& image);
    void statusChanged(Qn::ThumbnailStatus status);
    void sizeHintChanged(const QSize& sizeHint);

protected:
    virtual void doLoadAsync() = 0;
};

class BasicImageProvider: public ImageProvider
{
    Q_OBJECT
public:
    BasicImageProvider(const QImage& image, QObject* parent = nullptr);
    virtual ~BasicImageProvider() override = default;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

protected:
    virtual void doLoadAsync() override;

private:
    QImage m_image;
};

} // namespace nx::vms::client::desktop
