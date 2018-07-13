#pragma once

#include <QtCore/QSize>

#include <utils/common/watermark_settings.h>

class QPixmap;
class QPainter;

class QnWatermarkPainter
{
public:
    QnWatermarkPainter();

    void drawWatermark(QPainter* painter, const QRectF& rect);

    void setWatermarkText(const QString& text);
    void setWatermarkSettings(const QnWatermarkSettings& settings);
    void setWatermark(const QString& text, const QnWatermarkSettings& settings);

private:
    void updateWatermark();

    QString m_text;
    QPixmap m_pixmap;
    QnWatermarkSettings m_settings;

    QSize m_pixmapSize = QSize(0, 0);
};

