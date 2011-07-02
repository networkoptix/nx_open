#ifndef __AVI_DVD_STREAM_READER_H
#define __AVI_DVD_STREAM_READER_H

#include "avi_playlist_strem_dataprovider.h"


class CLAVIDvdStreamReader : public CLAVIPlaylistStreamReader
{
public:
	CLAVIDvdStreamReader(CLDevice* dev);
	virtual ~CLAVIDvdStreamReader();
    void setChapterNum(int chupter);
protected:
    virtual QStringList getPlaylist();
private:
    int m_chapter;
};

#endif __AVI_DVD_STREAM_READER_H
