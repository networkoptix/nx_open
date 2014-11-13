#ifndef QN_HELP_HANDLER_H
#define QN_HELP_HANDLER_H

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include "help_topics.h"

class QnHelpHandler: public QObject {
    Q_OBJECT;
public:
    QnHelpHandler(QObject *parent = NULL);
    virtual ~QnHelpHandler();
    
    void show();
    void hide();
    void setHelpTopic(int topic);

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    QUrl urlForTopic(int topic) const;

private:
    int m_topic;
    QString m_helpRoot;
    QString m_onlineHelpRoot;

    /** If the online help could not be loaded, try again in some time. */
    int m_helpRetryPeriodMSec;
};


#endif // QN_HELP_HANDLER_H
