#include "context_help_queryable.h"

#include <QtGui/QWidget>

#include <utils/common/warnings.h>

void setHelpTopicId(QWidget *widget, Qn::HelpTopic helpTopic) {
    if(!widget) {
        qnNullWarning(widget);
        return;
    }

    widget->setProperty(Qn::HelpTopicId, static_cast<int>(helpTopic));
}
