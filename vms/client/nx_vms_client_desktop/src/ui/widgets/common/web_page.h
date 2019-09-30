#pragma once

#include <memory>

#include <QtWebKitWidgets/QWebPage>

/**
 * A replacement for standard QWebPage.
 * It ignores SSL errors and disables HTTP/2.
 */
class QnWebPage: public QWebPage
{
    Q_OBJECT
    using base_type = QWebPage;

public:
    QnWebPage(QObject* parent = nullptr);

    bool supportsExtension(Extension extension) const override;
    bool extension(Extension extension,
        const ExtensionOption* option = 0,
        ExtensionReturn* output = 0) override;

protected:
    virtual void javaScriptConsoleMessage(
        const QString& message, int lineNumber, const QString& sourceID) override;

private:
    void download(QNetworkReply* reply);

    std::shared_ptr<QNetworkAccessManager> m_networkAccessManager;
};
