#ifndef QN_SERVER_SETTINGS_WIDGET_H
#define QN_SERVER_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>

namespace Ui {
    class ServerSettingsWidget;
}

class QnServerSettingsWidget: public QnAbstractPreferencesWidget {
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    QnServerSettingsWidget(QWidget *parent = NULL);
    virtual ~QnServerSettingsWidget();

    virtual void submitToSettings() override;
    virtual void updateFromSettings() override;

private:
    QScopedPointer<Ui::ServerSettingsWidget> ui;
};

#endif // QN_SERVER_SETTINGS_WIDGET_H
