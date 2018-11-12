#pragma once

#include <QtCore/QObject>
#include <QtCore/QSettings>
#include "qnaudio_device_info.h"

class QnAudioRecorderSettings: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit QnAudioRecorderSettings(QObject* parent = nullptr);
    virtual ~QnAudioRecorderSettings();

    QnAudioDeviceInfo primaryAudioDevice() const;

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceByName(const QString &name);

    QnAudioDeviceInfo secondaryAudioDevice() const;

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceByName(const QString &name);

    QString recordingFolder() const;
    void setRecordingFolder(QString folder);

    static QString getFullDeviceName(const QString& shortName);
    static QStringList availableDeviceNames(QAudio::Mode mode);
    static void splitFullName(const QString& name, QString& shortName, int& index);

private:
    QnAudioDeviceInfo getDeviceByName(const QString &name, QAudio::Mode mode,
        bool* isDefault = nullptr) const;

protected:
    QSettings settings;
};
