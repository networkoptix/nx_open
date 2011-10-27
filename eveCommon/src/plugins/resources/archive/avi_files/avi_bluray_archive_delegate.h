#ifndef __AVI_BLURAY_ARCHIVE_DELEGATE_H
#define __AVI_BLURAY_ARCHIVE_DELEGATE_H

#include <QStringList>
#include <QFile>
#include "avi_playlist_archive_delegate.h"



class QnAVIBlurayArchiveDelegate : public QnAVIPlaylistArchiveDelegate
{
public:
	QnAVIBlurayArchiveDelegate();
	virtual ~QnAVIBlurayArchiveDelegate();
protected:
    virtual QStringList getPlaylist(const QString& url);
    virtual void fillAdditionalInfo(CLFileInfo* fi);
};

#endif __AVI_BLURAY_ARCHIVE_DELEGATE_H
