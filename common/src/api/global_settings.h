#ifndef QN_GLOBAL_SETTINGS_H
#define QN_GLOBAL_SETTINGS_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>

template<class T>
class QnResourcePropertyAdaptor;

class QnGlobalSettings: public Connective<QObject>, public Singleton<QnGlobalSettings> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnGlobalSettings(QObject *parent = NULL);
    virtual ~QnGlobalSettings();

    QSet<QString> disabledVendorsSet() const;
    QString disabledVendors() const;
    void setDisabledVendors(QString disabledVendors);

    bool isCameraSettingsOptimizationEnabled() const;
    void setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled);

signals:
    void disabledVendorsChanged();
    void cameraSettingsOptimizationChanged();

private:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    mutable QMutex m_mutex;
    QnUserResourcePtr m_admin;
    QScopedPointer<QnResourcePropertyAdaptor<bool> > m_cameraSettingsOptimizationAdaptor;
    QScopedPointer<QnResourcePropertyAdaptor<QString> > m_disabledVendorsAdaptor;
};


#endif // QN_GLOBAL_SETTINGS_H
