// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_HELP_TOPIC_QUERYABLE_H
#define QN_HELP_TOPIC_QUERYABLE_H

#include <QtCore/QPointF>

class HelpTopicQueryable {
public:
    virtual ~HelpTopicQueryable() {}

    virtual int helpTopicAt(const QPointF &pos) const = 0;
};

#endif // QN_HELP_TOPIC_QUERYABLE_H
