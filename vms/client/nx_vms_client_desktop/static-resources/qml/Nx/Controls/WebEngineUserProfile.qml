pragma Singleton
import QtQuick 2.0
import QtWebEngine 1.7

WebEngineProfile
{
    // Store separate profile for each VMS user to avoid unauthorized access to websites opened
    // during another user session.
    storageName: workbench.context.userId
    // Web pages are re-created even on layout tab switch,
    // so profile shoud save as much data as possible.
    offTheRecord: false
    persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies

    onDownloadRequested: workbench.requestDownload(download)
}
