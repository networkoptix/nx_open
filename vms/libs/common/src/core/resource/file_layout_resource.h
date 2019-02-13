#pragma once

#include <common/common_globals.h>
#include <core/resource/layout_resource.h>

class QnFileLayoutResource: public QnLayoutResource
{
    Q_OBJECT

    using base_type = QnLayoutResource;
public:
    QnFileLayoutResource(QnCommonModule* commonModule = nullptr);

    virtual QString getUniqueId() const override;

    virtual Qn::ResourceStatus getStatus() const override;
    virtual void setStatus(Qn::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    virtual bool isFile() const override;

private:
    Qn::ResourceStatus m_localStatus;
};

Q_DECLARE_METATYPE(QnFileLayoutResourcePtr);
Q_DECLARE_METATYPE(QnFileLayoutResourceList);
