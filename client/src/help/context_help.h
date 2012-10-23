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
    void setHelpTopic(Qn::HelpTopic topic);

protected:
    QUrl urlForTopic(Qn::HelpTopic topic) const;

private:
    Qn::HelpTopic m_topic;
    QString m_helpRoot;
};


#endif // QN_CONTEXT_HELP_H
