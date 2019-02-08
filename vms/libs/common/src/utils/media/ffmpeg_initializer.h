#pragma once

#include <QtCore/QObject>

/** RAII class for correct ffmpeg initializing and deinitializing. */
class QnFfmpegInitializer: public QObject
{
    Q_OBJECT

public:
    QnFfmpegInitializer(QObject* parent = nullptr);
    ~QnFfmpegInitializer();
};
