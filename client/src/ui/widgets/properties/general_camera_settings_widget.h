#ifndef QN_GENERAL_CAMERA_SETTINGS_WIDGET_H
#define QN_GENERAL_CAMERA_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <ui/models/camera_settings_model.h>

#include <utils/common/connective.h>

namespace Ui {
    class GeneralCameraSettingsWidget;
}

class QnGeneralCameraSettingsWidget : public Connective<QWidget>, public QnWorkbenchContextAware {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef Connective<QWidget> base_type;
public:
    QnGeneralCameraSettingsWidget(QWidget* parent = 0);
    virtual ~QnGeneralCameraSettingsWidget();

    QnCameraSettingsModel* model() const;
    void setModel(QnCameraSettingsModel* model);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResources();
    void submitToResources();
signals:
    void dataChanged();
    void fisheyeSettingsChanged();
    void webPageChanged(const QString &webPageUrl);
    void recordingStateChanged();

private:
    void updateLicensesButtonVisible();
    void updateIpAddressText();
    void updateWebPageText();

    void at_dataChanged();
    void at_linkActivated(const QString &urlString);
    void at_enableAudioCheckBox_clicked();
    void at_analogViewCheckBox_clicked();
    void at_pingButton_clicked();

private:
    QScopedPointer<Ui::GeneralCameraSettingsWidget> ui;
    bool m_updating;
    bool m_readOnly;
    QnVirtualCameraResourceList m_cameras;
};

#endif // QN_GENERAL_CAMERA_SETTINGS_WIDGET_H
