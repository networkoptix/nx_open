#ifndef __VIDEO_SERVER_H
#define __VIDEO_SERVER_H

#include "url_resource.h"

class QnVideoServer: public QnURLResource
{
public:
    virtual QnResourcePtr updateResource() { return QnResourcePtr(this); }
    virtual bool getBasicInfo() { return true; }
    virtual void beforeUse() {}

    virtual QString manufacture() const { return "Video server"; }
};

#endif
