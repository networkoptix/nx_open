#ifndef MOBILE_CLIENT_SETTINGS_H
#define MOBILE_CLIENT_SETTINGS_H

#include <utils/common/property_storage.h>
#include <utils/common/singleton.h>
#include <mobile_client/mobile_client_meta_types.h>

class QnMobileClientSettings : public QnPropertyStorage, public Singleton<QnMobileClientSettings> {
    Q_OBJECT
    typedef QnPropertyStorage base_type;

public:
    enum Variable {
        SavedSessions,
        VariableCount
    };

    explicit QnMobileClientSettings(QObject *parent = 0);

    void load();
    void save();

    bool isWritable() const;

protected:
    virtual void updateValuesFromSettings(QSettings *settings, const QList<int> &ids) override;

    virtual QVariant readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) override;
    virtual void writeValueToSettings(QSettings *settings, int id, const QVariant &value) const override;

    virtual UpdateStatus updateValue(int id, const QVariant &value) override;

private:
    QN_BEGIN_PROPERTY_STORAGE(VariableCount)
        QN_DECLARE_RW_PROPERTY(QVariantList,            savedSessions,      setSavedSessions,       SavedSessions,        QVariantList())
    QN_END_PROPERTY_STORAGE()

private:
    QSettings *m_settings;
    bool m_loading;
};

#define qnSettings (QnMobileClientSettings::instance())

#endif // MOBILE_CLIENT_SETTINGS_H
