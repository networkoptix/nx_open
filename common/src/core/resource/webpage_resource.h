#pragma once

#include <core/resource/resource.h>

// TODO: #GDM Move to the corresponding library in the 4.0
namespace nx {
namespace vms {
namespace api {

enum class WebPageSubtype
{
    none,
    c2p
};

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::WebPageSubtype)

class QnWebPageResource : public QnResource
{
    Q_OBJECT
    using base_type = QnResource;

public:
    QnWebPageResource(QnCommonModule* commonModule = nullptr);

    virtual ~QnWebPageResource();

    virtual void setUrl(const QString& url) override;

    static QString nameForUrl(const QUrl& url);

    virtual Qn::ResourceStatus getStatus() const override;
    virtual void setStatus(Qn::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    /** Check if the page is the special integration page. */
    nx::vms::api::WebPageSubtype subtype() const;
    void setSubtype(nx::vms::api::WebPageSubtype value);

signals:
    void subtypeChanged(const QnWebPageResourcePtr& webPage);

private:
    // This class must not store its status on server side
    Qn::ResourceStatus m_status{Qn::NotDefined};
};

Q_DECLARE_METATYPE(QnWebPageResourcePtr)
Q_DECLARE_METATYPE(QnWebPageResourceList)
