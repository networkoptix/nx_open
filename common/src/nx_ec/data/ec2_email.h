#ifndef __EC2__EMAIL_DATA_H_
#define __EC2__EMAIL_DATA_H_

#include <vector>

#include "api_data.h"
#include "utils/common/email.h"
#include <api/model/email_attachment.h>


namespace ec2
{
    class ApiEmailSettingsData
    :
        public ApiData
    {
    public:
        QString host;
        int port;
        QString user;
        QString from;
        QString password;
        QnEmail::ConnectionType connectionType;

        ApiEmailSettingsData();

        void fromResource( const QnEmail::Settings& settings );
        void toResource( QnEmail::Settings& settings ) const;
    };

	#define ApiEmailSettingsFields (host)(port)(user)(password)(connectionType)
	QN_DEFINE_STRUCT_SERIALIZATORS( ApiEmailSettingsData, ApiEmailSettingsFields )
	
    class ApiEmailData
    :
        public ApiData
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
	QN_DEFINE_STRUCT_SERIALIZATORS( ApiEmailData, ApiEmailFields )
}

#endif // __EC2__EMAIL_DATA_H_