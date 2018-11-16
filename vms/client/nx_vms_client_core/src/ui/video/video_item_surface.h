#pragma once

#include <QtMultimedia/QAbstractVideoSurface>

class QnVideoOutputBackend;

class QnVideoItemSurface: public QAbstractVideoSurface
{
    Q_OBJECT

public:
    explicit QnVideoItemSurface(QnVideoOutputBackend* backend, QObject* parent = nullptr);
    ~QnVideoItemSurface();

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType) const;
    bool start(const QVideoSurfaceFormat& format) override;
    void stop() override;
    bool present(const QVideoFrame& frame) override;

    void scheduleOpenGLContextUpdate();

private slots:
    void updateOpenGLContext();

private:
    QnVideoOutputBackend* m_backend;
};
