#ifndef YOUTUBESETTINGSWIDGET_H
#define YOUTUBESETTINGSWIDGET_H

#include <QtGui/QWidget>

namespace Ui {
    class YouTubeSettings;
}

class QnYouTubeSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnYouTubeSettingsWidget(QWidget *parent = 0);
    ~QnYouTubeSettingsWidget();

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
