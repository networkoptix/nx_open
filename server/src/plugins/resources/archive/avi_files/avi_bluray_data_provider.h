#ifndef __AVI_BLURAY_STREAM_READER_H
#define __AVI_BLURAY_STREAM_READER_H

#include <QStringList>
#include <QFile>
#include "avi_playlist_strem_dataprovider.h"


class CLAVIBlurayStreamReader : public CLAVIPlaylistStreamReader
{
public:
	CLAVIBlurayStreamReader(CLDevice* dev);
	virtual ~CLAVIBlurayStreamReader();
protected:
    virtual QStringList getPlaylist();
};

#endif __AVI_BLURAY_STREAM_READER_H
