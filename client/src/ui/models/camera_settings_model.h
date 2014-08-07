#ifndef CAMERA_SETTINGS_MODEL_H
#define CAMERA_SETTINGS_MODEL_H

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnCameraSettingsModel: public Connective<QObject> {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnCameraSettingsModel(QObject *parent = NULL);
    ~QnCameraSettingsModel();

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    QString name() const;
    void setName(const QString &name);

    QString model() const;
    QString firmware() const;
    QString vendor() const;

    QString address() const;
    void setAddress(const QString &address);

    QString webPage() const;
    void setWebPage(const QString &webPage);

signals:
    void addressChanged(const QString &address);
    void webPageChanged(const QString &webPageUrl);

private:
    void at_camera_urlChanged(const QnResourcePtr &resource);

private:
    QnVirtualCameraResourcePtr m_camera;
    bool m_updating;

    QString m_address;
    QString m_webPage;

}

#endif //CAMERA_SETTINGS_MODEL_H
