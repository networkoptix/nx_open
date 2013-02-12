#ifndef QN_SERVER_SETTINGS_WIDGET_H
#define QN_SERVER_SETTINGS_WIDGET_H

#include <QtGui/QWidget>

namespace Ui {
    class ServerSettingsWidget;
}

class QnServerSettingsWidget: public QWidget {
    Q_OBJECT;
public:
    QnServerSettingsWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnServerSettingsWidget();

    void submit();
    void update();

private:
    QScopedPointer<Ui::ServerSettingsWidget> ui;
};

#endif // QN_SERVER_SETTINGS_WIDGET_H
