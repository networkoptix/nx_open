#include "popup_business_action.h"

namespace BusinessActionParameters {
    static QLatin1String userGroup("userGroup");

    quint64 getUserGroup(const QnBusinessParams &params) {
        return params.value(userGroup, 0).toULongLong();
    }

    void setUserGroup(QnBusinessParams* params, const quint64 value) {
        (*params)[userGroup] = value;
    }

}

QnPopupBusinessAction::QnPopupBusinessAction(const QnBusinessParams &runtimeParams):
    base_type(BusinessActionType::ShowPopup, runtimeParams)
{
}
