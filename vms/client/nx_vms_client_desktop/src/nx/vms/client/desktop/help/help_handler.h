// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include "help_topic.h"

namespace nx::vms::client::desktop {

// This class should not depend on any singletons.
// If it must, it should be changed to our QnSingleton descendant.
class HelpHandler: public QObject
{
    Q_OBJECT

public:
    HelpHandler(QObject* parent = nullptr);
    virtual ~HelpHandler() override;

    // Sets help topic and opens a browser for it.
    void setHelpTopic(int topic);
    void setHelpTopic(HelpTopic::Id topic);

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

    Q_INVOKABLE static void openHelpTopic(int topic);
    Q_INVOKABLE static void openHelpTopic(HelpTopic::Id topic);

    static HelpHandler& instance();

protected:
    static QUrl urlForTopic(HelpTopic::Id topic);

private:
    HelpTopic::Id m_topic = HelpTopic::Id::Empty;
};

} // namespace nx::vms::client::desktop
