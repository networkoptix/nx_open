#ifndef _COMMON_MUSTACHE_PARTIAL_INFO_H_
#define _COMMON_MUSTACHE_PARTIAL_INFO_H_

#ifdef ENABLE_SENDMAIL
// TODO: #Elric does not belong in mustache. And the naming is awful. What kind of partial info?

#include "api/model/email_attachment.h"
#include "business/events/abstract_business_event.h"

struct QnPartialInfo {
    QnPartialInfo(QnBusiness::EventType value);

    QString attrName;
    QnEmailAttachmentList attachments;
    QString eventLogoFilename;
};

#endif // ENABLE_SENDMAIL

#endif // _COMMON_MUSTACHE_PARTIAL_INFO_H_
