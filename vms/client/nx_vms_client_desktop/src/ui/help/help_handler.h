#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include "help_topics.h"

// This class should not depend on any singletons.
// If it must, it should be changed to our QnSingleton descendant.
class QnHelpHandler: public QObject
{
    Q_OBJECT

public:
    QnHelpHandler(QObject* parent = nullptr);
    virtual ~QnHelpHandler();

    // Sets help topic and opens a browser for it.
    void setHelpTopic(int topic);

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

    Q_INVOKABLE static void openHelpTopic(int topic);

    static QnHelpHandler& instance();

protected:
    static QUrl urlForTopic(int topic);

private:
    int m_topic;
};
