#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include "help_topics.h"

class QnHelpHandler: public QObject
{
    Q_OBJECT
public:
    QnHelpHandler(QObject* parent = nullptr);
    virtual ~QnHelpHandler();

    void setHelpTopic(int topic);

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    QUrl urlForTopic(int topic) const;

private:
    int m_topic;
    QStringList m_helpSearchPaths;
};
