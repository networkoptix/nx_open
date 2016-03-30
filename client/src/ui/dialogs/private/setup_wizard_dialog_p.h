#pragma once

#include <QtWebKitWidgets/QWebView>

class QnSetupWizardDialog;

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

    void updateCredentials(const QString& login, const QString& password, bool isCloud);

    void cancel();
public:
    struct LoginInfo
    {
        QString cloudEmail;
        QString cloudPassword;
        QString localLogin;
        QString localPassword;
    };

#define LoginInfo_Fields (cloudEmail)(cloudPassword)(localLogin)(localPassword)

    QWebView *webView;
    QUrl url;
    LoginInfo loginInfo;
};
