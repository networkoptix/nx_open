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
        VariableCount
    };

    explicit QnMobileClientSettings(QObject *parent = 0);

    void load();
    void save();
};

#define qnSettings (QnMobileClientSettings::instance())

#endif // MOBILE_CLIENT_SETTINGS_H
