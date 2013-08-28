#include "email_attachment.h"

#include <QtCore/QFile>

QnEmailAttachment::QnEmailAttachment(const QString &filename_, QIODevice& io_, const QString &mimetype_)
    : filename(filename_),
    mimetype(mimetype_){
    io_.open(QIODevice::ReadOnly);
    content = io_.readAll();
}

QnEmailAttachment::QnEmailAttachment(const QString &filename_, const QString &contentFilename, const QString &mimetype_)
    : filename(filename_),
    mimetype(mimetype_) {

    QFile contentFile(contentFilename);
    contentFile.open(QIODevice::ReadOnly);
    content = contentFile.readAll();
}


