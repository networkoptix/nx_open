#include <QString>

class IMMDeviceEnumerator;

class WinAudioExtendInfo
{
public:
    WinAudioExtendInfo(const QString& name);
    virtual ~WinAudioExtendInfo();

    QString fullName() const { return m_fullName; }
    bool isMicrophone() const;
    QPixmap deviceIcon() const;
private:
    QString m_fullName;
    GUID m_jackSubType;
    QString m_iconPath;
    IMMDeviceEnumerator* m_pMMDeviceEnumerator;
};
