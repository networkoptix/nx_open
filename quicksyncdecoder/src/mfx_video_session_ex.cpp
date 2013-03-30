/**********************************************************
* 05 feb 2013
* a.kolesnikov
***********************************************************/

#include "mfx_video_session_ex.h"

#include <QMutex>
#include <QMutexLocker>


//synchronizing some operations due to bad parallelization
static QMutex globalMFXMutex;

mfxStatus MFXVideoSessionEx::Init( mfxIMPL impl, mfxVersion* ver )
{
    QMutexLocker lk( &globalMFXMutex );
    return MFXVideoSession::Init( impl, ver );
}

mfxStatus MFXVideoSessionEx::Close()
{
    QMutexLocker lk( &globalMFXMutex );
    return MFXVideoSession::Close();
}

mfxSession& MFXVideoSessionEx::getInternalSession()
{
    return m_session;
}
