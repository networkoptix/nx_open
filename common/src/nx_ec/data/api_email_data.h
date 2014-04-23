#ifndef QN_API_EMAIL_DATA_H
#define QN_API_EMAIL_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    class ApiEmailSettingsData: ApiData
    {
    public:
        ApiEmailSettingsData(): port(0) {}

        QString host;
        int port;
        QString user;
        QString from;
        QString password;
        QnEmail::ConnectionType connectionType;

        /*void fromResource( const QnEmail::Settings& settings );
        void toResource( QnEmail::Settings& settings ) const;* /*/
    };
#define ApiEmailSettingsData_Fields (host)(port)(user)(password)(connectionType)
	
    //QN_DEFINE_STRUCT_SERIALIZATORS( ApiEmailSettingsData, ApiEmailSettingsFields )
    //QN_FUSION_DECLARE_FUNCTIONS(ApiEmailSettingsData, (binary))
	
    class ApiEmailData: ApiData
    {
    public:
        ApiEmailData() {}

        ApiEmailData (const QStringList& to_, const QString& subject_, const QString& body_, int timeout_, const QnEmailAttachmentList& attachments_)
            : to(to_),
            subject(subject_),
            body(body_),
            timeout(timeout_),
            attachments(attachments_)
        {}

        QStringList to;
        QString subject;
        QString body;
        int timeout;

        QnEmailAttachmentList attachments;
    };

	#define ApiEmailFields (to)(subject)(body)(timeout)
	QN_FUSION_DECLARE_FUNCTIONS(ApiEmailData, (binary))
}

#endif // QN_API_EMAIL_DATA_H