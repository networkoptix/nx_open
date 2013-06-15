#ifndef __ADVANCED_SETTINGS_WIDGET_H__
#define __ADVANCED_SETTINGS_WIDGET_H__

namespace Ui {
    class AdvancedSettingsWidget;
}


class QnAdvancedSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    QnAdvancedSettingsWidget(QWidget* parent = 0);
    virtual ~QnAdvancedSettingsWidget();
private:
	Q_DISABLE_COPY(QnAdvancedSettingsWidget)

    QScopedPointer<Ui::AdvancedSettingsWidget> ui;
};

#endif // __ADVANCED_SETTINGS_WIDGET_H__
