#include <plugins/resource/onvif/onvif_resource.h>
#include <utils/network/http/httpclient.h>

class QnOpteraResource : public QnPlOnvifResource
{
    typedef QnPlOnvifResource base_type;
public:
    QnOpteraResource();
    virtual ~QnOpteraResource();

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    CLHttpStatus makeGetStitchingModeRequest(CLSimpleHTTPClient& client, QByteArray& response) const;
    CLHttpStatus makeSetStitchingModeRequest(CLSimpleHTTPClient& http, const QString& mode) const;
    
    QString getCurrentStitchingMode(const QByteArray& page) const; 

};