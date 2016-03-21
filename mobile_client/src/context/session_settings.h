#pragma once

#include <utils/common/property_storage.h>
#include <mobile_client/mobile_client_meta_types.h>

class QnSessionSettings : public QnPropertyStorage {
    Q_OBJECT
    typedef QnPropertyStorage base_type;

public:
    enum Variable {
        HiddenCameras,
        HiddenCamerasCollapsed,

        VariableCount
    };

    QnSessionSettings(QObject *parent, const QString &sessionId);

    static QString settingsFileName(const QString &sessionId);

protected:
    virtual void updateValuesFromSettings(QSettings *settings, const QList<int> &ids) override;

    virtual UpdateStatus updateValue(int id, const QVariant &value) override;

private:
    QN_BEGIN_PROPERTY_STORAGE(VariableCount)
    QN_DECLARE_RW_PROPERTY(QnStringSet,         hiddenCameras,                      setHiddenCameras,                   HiddenCameras,                      QnStringSet())
    QN_DECLARE_RW_PROPERTY(bool,                isHiddenCamerasCollapsed,           setHiddenCamerasCollapsed,          HiddenCamerasCollapsed,             true)
    QN_END_PROPERTY_STORAGE()

private:
    QSettings *m_settings;
    bool m_loading;
};
