#ifndef QN_WIN_AUDIO_EXTEND_INFO_H
#define QN_WIN_AUDIO_EXTEND_INFO_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <Windows.h>

#include <QtCore/QString>

struct IMMDeviceEnumerator;
struct IMMDevice;

class WinAudioExtendInfo {
public:
    WinAudioExtendInfo(const QString& name);
    virtual ~WinAudioExtendInfo();

    QString fullName() const { return m_fullName; }
    bool isMicrophone() const;
    QPixmap deviceIcon();

private:
    bool getDeviceInfo(IMMDevice *pMMDevice, bool isDefault);

private:
    int m_iconGroupIndex;
    int m_iconGroupNum;
    QString m_deviceName;
    QString m_fullName;
    GUID m_jackSubType;
    QString m_iconPath;
    IMMDeviceEnumerator *m_pMMDeviceEnumerator;

    friend BOOL CALLBACK enumFunc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam);
};

#endif // Q_OS_WIN

#endif // QN_WIN_AUDIO_EXTEND_INFO_H
