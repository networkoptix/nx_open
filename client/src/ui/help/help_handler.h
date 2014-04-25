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

private slots:
    void at_helpUrlDetector_urlFetched(const QString &helpUrl);
    void at_helpUrlDetector_error();

private:
    int m_topic;
    QString m_helpRoot;
    QString m_onlineHelpRoot;
};


#endif // QN_HELP_HANDLER_H
