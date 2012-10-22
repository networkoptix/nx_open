#include "context_help_queryable.h"

#include <QtGui/QWidget>

#include <utils/common/warnings.h>

void setHelpTopicId(QWidget *widget, int helpTopic) {
    if(!widget) {
        qnNullWarning(widget);
        return;
    }

    widget->setProperty(Qn::HelpTopicId, helpTopic);
}
