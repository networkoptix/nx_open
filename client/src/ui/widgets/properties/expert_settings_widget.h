#ifndef QN_ADVANCED_SETTINGS_WIDGET_H
#define QN_ADVANCED_SETTINGS_WIDGET_H

#include <QtGui/QWidget>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class AdvancedSettingsWidget;
}

class QnAdvancedSettingsWidget : public QWidget {
    Q_OBJECT
public:
    QnAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~QnAdvancedSettingsWidget();

    void updateFromResources(const QnVirtualCameraResourceList &cameras);
    void submitToResources(const QnVirtualCameraResourceList &cameras);

signals:
    void dataChanged();

private slots:
    void at_dataChanged();

    void at_restoreDefaultsButton_clicked();
    void at_qualitySlider_valueChanged(int value);

    /** Update UI controls related to camera settings group */
    void at_settingsAssureCheckBox_toggled(bool checked);

    /** Update UI controls related to second stream quality group */
    void at_qualityAssureCheckBox_toggled(bool checked);

private:
    bool isArecontCamera(const QnVirtualCameraResourcePtr &camera) const;
    bool isDefaultValues() const;

    QScopedPointer<Ui::AdvancedSettingsWidget> ui;
    bool m_updating;
};

#endif // QN_ADVANCED_SETTINGS_WIDGET_H
