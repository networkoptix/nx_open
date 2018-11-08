/**********************************************************
* 05 feb 2013
* a.kolesnikov
***********************************************************/

#ifndef MFX_VIDEO_SESSION_EX_H
#define MFX_VIDEO_SESSION_EX_H

#include <mfxvideo++.h>


class MFXVideoSessionEx
:
    public MFXVideoSession
{
public:
    mfxStatus Init( mfxIMPL impl, mfxVersion* ver );
    mfxStatus Close();

    mfxSession& getInternalSession();
};

#endif  //MFX_VIDEO_SESSION_EX_H
