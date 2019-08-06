#pragma once

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

protected:
    virtual void javaScriptConsoleMessage(
        const QString& message, int lineNumber, const QString& sourceID) override;
};
