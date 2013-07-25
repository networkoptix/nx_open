#include "utils/fs/fileutil.h"
#include "api/model/email_attachment.h"

QnEmailAttachment::QnEmailAttachment(const QString &filename_, const QString &contentFilename, const QString &mimetype_)
    : filename(filename_),
    mimetype(mimetype_) {

    QFile contentFile(contentFilename);
    contentFile.open(QIODevice::ReadOnly);
    content = contentFile.readAll();
}
