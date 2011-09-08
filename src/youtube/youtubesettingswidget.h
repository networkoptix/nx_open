#ifndef YOUTUBESETTINGSWIDGET_H
#define YOUTUBESETTINGSWIDGET_H

#include <QtGui/QWidget>

namespace Ui {
    class YouTubeSettings;
}

class YouTubeSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit YouTubeSettingsWidget(QWidget *parent = 0);
    ~YouTubeSettingsWidget();

    static QString login();
    static QString password();

public Q_SLOTS:
    void accept();

private Q_SLOTS:
    void authPairChanged();
    void tryAuth();
    void authFailed();
    void authFinished();

private:
    Ui::YouTubeSettings *ui;
};

#endif // YOUTUBESETTINGSWIDGET_H
