#ifndef IEXPLORE_URL_HANDLER_H
#define IEXPLORE_URL_HANDLER_H

#include <QUrl>

/**
 * Workaround class that fixes an Internet Explorer bug. IE 9.0 could not properly
 * handle links that include username and/or password, so they are removed if user's
 * default browser is detected as IE.
 */
class QnIexploreUrlHandler: public QObject{
    Q_OBJECT
public:
    QnIexploreUrlHandler();
    virtual ~QnIexploreUrlHandler();
public slots:
    bool handleUrl(QUrl url);
};

#endif // IEXPLORE_URL_HANDLER_H
