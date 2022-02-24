// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    void setHelpTopic(Qn::HelpTopic topic);

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

    Q_INVOKABLE static void openHelpTopic(int topic);
    Q_INVOKABLE static void openHelpTopic(Qn::HelpTopic topic);

    static QnHelpHandler& instance();

protected:
    static QUrl urlForTopic(Qn::HelpTopic topic);

private:
    Qn::HelpTopic m_topic;
};
