#include "webpage_resource.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/webpage_data.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, WebPageSubtype,
    (nx::vms::api::WebPageSubtype::none, QString())
    (nx::vms::api::WebPageSubtype::c2p, "c2p")
)

namespace {

static const QString kSubtypePropertyName = lit("subtype");

} // namespace

using namespace nx::vms::api;

QnWebPageResource::QnWebPageResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
    setTypeId(nx::vms::api::WebPageData::kResourceTypeId);
    addFlags(Qn::web_page);

    connect(this, &QnResource::propertyChanged, this,
        [this](const QnResourcePtr& /*resource*/, const QString& key)
        {
            if (key == kSubtypePropertyName)
                emit subtypeChanged(toSharedPointer(this));
        });
}

QnWebPageResource::~QnWebPageResource()
{
}

void QnWebPageResource::setUrl(const QString& url)
{
    base_type::setUrl(url);
    setStatus(Qn::Online);
}

QString QnWebPageResource::nameForUrl(const QUrl& url)
{
    QString name = url.host();
    if (!url.path().isEmpty())
        name += L'/' + url.path();
    return name;
}

Qn::ResourceStatus QnWebPageResource::getStatus() const
{
    QnMutexLocker lock(&m_mutex);
    return m_status;
}

void QnWebPageResource::setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_status == newStatus)
            return;
        m_status = newStatus;
    }
    emit statusChanged(toSharedPointer(), reason);
}

WebPageSubtype QnWebPageResource::subtype() const
{
    return QnLexical::deserialized<WebPageSubtype>(getProperty(kSubtypePropertyName));
}

void QnWebPageResource::setSubtype(WebPageSubtype value)
{
    setProperty(kSubtypePropertyName, QnLexical::serialized(value));
}
