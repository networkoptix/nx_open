#ifndef QN_AVI_BLURAY_ARCHIVE_DELEGATE_H
#define QN_AVI_BLURAY_ARCHIVE_DELEGATE_H

#ifdef ENABLE_ARCHIVE

#include <QtCore/QStringList>
#include <QtCore/QFile>

#include "avi_playlist_archive_delegate.h"

class QnAVIBlurayArchiveDelegate : public QnAVIPlaylistArchiveDelegate
{
    Q_OBJECT;

public:
    QnAVIBlurayArchiveDelegate();
    virtual ~QnAVIBlurayArchiveDelegate();
protected:
    virtual QStringList getPlaylist(const QString& url);
    virtual void fillAdditionalInfo(CLFileInfo* fi);
};

#endif // ENABLE_ARCHIVE

#endif // QN_AVI_BLURAY_ARCHIVE_DELEGATE_H
