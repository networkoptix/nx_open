#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

class QnSetupWizardDialog;
class QQuickWidget;

class QnSetupWizardDialogPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QnSetupWizardDialog)
    QnSetupWizardDialog *q_ptr;

public:
    QnSetupWizardDialogPrivate(QnSetupWizardDialog *parent);

public slots:
    void openUrlInBrowser(const QString &urlString);

    QString getCredentials() const;

    void updateCredentials(const QString& login,
        const QString& password,
        bool isCloud,
        bool savePassword);

    void cancel();

    void load(const QUrl& url);

public:
    struct LoginInfo
    {
        QString cloudEmail;
        QString cloudPassword;
        QString localLogin;
        QString localPassword;
        bool savePassword;
    };

#define LoginInfo_Fields (cloudEmail)(cloudPassword)(localLogin)(localPassword)(savePassword)

    QQuickWidget* m_quickWidget;
    QUrl url;
    LoginInfo loginInfo;
};
