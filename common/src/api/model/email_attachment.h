#ifndef _API_MODEL_EMAIL_ATTACHMENT_
#define _API_MODEL_EMAIL_ATTACHMENT_

#include <QString>
#include <QList>
#include <QSharedPointer>

struct QnEmailAttachment {
    QnEmailAttachment(const QString &filename_, const QString &contentFilename, const QString &mimetype_);
    QnEmailAttachment(const QString &filename_, QIODevice& io_, const QString &mimetype_);

    QString filename;
    QByteArray content;
    QString mimetype;
};

typedef QSharedPointer<QnEmailAttachment> QnEmailAttachmentPtr;
typedef QList<QnEmailAttachmentPtr> QnEmailAttachmentList;

#endif // _API_MODEL_EMAIL_ATTACHMENT_
