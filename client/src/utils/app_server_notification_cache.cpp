#include "app_server_notification_cache.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <transcoding/file_transcoder.h>

namespace {
    const QLatin1String targetContainter("mp3");

    const unsigned int durationMs = 10000;
}

QnAppServerNotificationCache::QnAppServerNotificationCache(QObject *parent) :
    base_type(parent)
{
}

QnAppServerNotificationCache::~QnAppServerNotificationCache() {

}

void QnAppServerNotificationCache::storeSound(const QString &filePath) {
    QString uuid = QUuid::createUuid().toString();
    QString newFilename = uuid.mid(1, uuid.size() - 2) + QLatin1String(".mp3");
    QString title = QFileInfo(filePath).fileName();
    title = title.mid(0, title.lastIndexOf(QLatin1Char('.')));

    FileTranscoder* transcoder = new FileTranscoder();
    transcoder->setSourceFile(filePath);
    transcoder->setDestFile(getFullPath(newFilename));
    transcoder->setContainer(targetContainter);
    transcoder->setAudioCodec(CODEC_ID_MP3);
    transcoder->setTranscodeDurationLimit(durationMs);
    transcoder->addTag(QLatin1String("Comment"), title);

    connect(transcoder, SIGNAL(done(QString)), this, SLOT(at_soundConverted(QString)));
    connect(transcoder, SIGNAL(done(QString)), transcoder, SLOT(deleteLater()));
    transcoder->startAsync();
}

void QnAppServerNotificationCache::at_soundConverted(const QString &filePath) {
    uploadFile(QFileInfo(filePath).fileName());
}
