#ifndef QN_IEXPLORE_URL_HANDLER_H
#define QN_IEXPLORE_URL_HANDLER_H

#include <QtCore/QObject>
#include <QtCore/QUrl>

/**
 * Workaround class that fixes an Internet Explorer bug. IE 9.0 could not properly
 * handle links that include username and/or password, so they are removed if user's
 * default browser is detected as IE.
 */
class QnIexploreUrlHandler: public QObject {
    Q_OBJECT
public:
    QnIexploreUrlHandler(QObject *parent = NULL);
    virtual ~QnIexploreUrlHandler();

public slots:
    bool handleUrl(QUrl url);
};

#endif // QN_IEXPLORE_URL_HANDLER_H
