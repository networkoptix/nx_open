#pragma once

#include <core/resource/resource.h>

class QnWebPageResource: public QnResource {
    Q_OBJECT
    typedef QnResource base_type;
public:
    QnWebPageResource();
    virtual ~QnWebPageResource();

};

Q_DECLARE_METATYPE(QnWebPageResourcePtr)
Q_DECLARE_METATYPE(QnWebPageResourceList)