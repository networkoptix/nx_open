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

public:
    QWebView *webView;
};
