#include "toggle_business_action.h"

class QnRecordingBusinessAction: public QnToggleBusinessAction
{
public:
    QnRecordingBusinessAction();

    virtual bool execute() override;
    virtual QByteArray QnRecordingBusinessAction::serialize() override;
    virtual bool QnRecordingBusinessAction::deserialize(const QByteArray& data) override;

};
