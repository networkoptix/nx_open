#ifndef __AVI_BLURAY_STREAM_READER_H
#define __AVI_BLURAY_STREAM_READER_H

#include <QStringList>
#include <QFile>

#include "avi_playlist_strem_reader.h"

class CLAVIBlurayStreamReader : public CLAVIPlaylistStreamReader
{
public:
	CLAVIBlurayStreamReader(CLDevice* dev);
	virtual ~CLAVIBlurayStreamReader();
protected:
    virtual QStringList getPlaylist();
    virtual void fillAdditionalInfo(CLFileInfo* fi);
};

#endif __AVI_BLURAY_STREAM_READER_H
