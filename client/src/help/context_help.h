#ifndef QN_CONTEXT_HELP_H
#define QN_CONTEXT_HELP_H

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include "help_topics.h"

class QnContextHelp: public QObject {
    Q_OBJECT;
public:
    QnContextHelp(QObject *parent = NULL);
    virtual ~QnContextHelp();
    
    void show();
    void hide();
    void setHelpTopic(int topic);

protected:
    QUrl urlForTopic(int topic) const;

private:
    int m_topic;
    QString m_helpRoot;
};


#endif // QN_CONTEXT_HELP_H
