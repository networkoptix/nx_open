#include "update_info.h"

#include <utils/common/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnUpdateInfo, (datastream), (currentRelease)(releaseNotesUrl)(releaseDateMs)(releaseDeliveryDays))

QnUpdateInfo::QnUpdateInfo()
  : currentRelease()
  , releaseNotesUrl()
  , releaseDateMs(0)
  , releaseDeliveryDays(0)
{

}
