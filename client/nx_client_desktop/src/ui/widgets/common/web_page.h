#pragma once

#include <QtWebKitWidgets/QWebPage>

/* Replacement common class */
class QnWebPage : public QWebPage
{
    Q_OBJECT

    typedef QWebPage base_type;
public:
    QnWebPage(QObject* parent = 0);

protected:
    virtual void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID) override;

};