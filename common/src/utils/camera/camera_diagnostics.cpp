/**********************************************************
* 23 jul 2013
* a.kolesnikov
***********************************************************/

#include "camera_diagnostics.h"


namespace CameraDiagnostics
{
    namespace DiagnosticsStep
    {
        QString toString( Value val )
        {
            switch( val )
            {
                case none:
                    return QString::fromLatin1("none");
                case mediaServerAvailability:
                    return QString::fromLatin1("mediaServerAvailability");
                case cameraAvailability:
                    return QString::fromLatin1("cameraAvailability");
                case mediaStreamAvailability:
                    return QString::fromLatin1("mediaStreamAvailability");
                case mediaStreamIntegrity:
                    return QString::fromLatin1("mediaStreamIntegrity");
                default:
                    return QString::fromLatin1("unknown");
            }
        }

        Value fromString( const QString& str )
        {
            if( str == QString::fromLatin1("mediaServerAvailability") )
                return mediaServerAvailability;
            else if( str == QString::fromLatin1("cameraAvailability") )
                return cameraAvailability;
            else if( str == QString::fromLatin1("mediaStreamAvailability") )
                return mediaStreamAvailability;
            else if( str == QString::fromLatin1("mediaStreamIntegrity") )
                return mediaStreamIntegrity;
            else
                return none;
        }
    }

    namespace ErrorCode
    {
        //!Returns textual description of error with  parameters
        QString toString( Value val, const QList<QString>& errorParams )
        {
            int requiredParamCount = 0;
            QString errorMessage;
            switch( val )
            {
                case noError:
                    requiredParamCount = 0;
                    errorMessage = QString::fromLatin1("ok");
                    break;
                case cannotEstablishConnection:
                    requiredParamCount = 1;
                    errorMessage = QString::fromLatin1("Camera %1 is unavailable");
                    break;
                case cannotOpenCameraMediaPort:
                    requiredParamCount = 2;
                    errorMessage = QString::fromLatin1("Cannot connect to media port %1 of camera %2");
                    break;
                default:
                    errorMessage = QString::fromLatin1("Unknown error. Parameters: ");
                    for( int i = 0; i < errorParams.size(); ++i )
                    {
                        if( i > 0 )
                            errorMessage += QLatin1String(", ");
                        errorMessage += errorParams[i];
                    }
                    break;
            }

            requiredParamCount = std::min<int>(requiredParamCount, errorParams.size());
            for( int i = 0; i < requiredParamCount; ++i )
                errorMessage = errorMessage.arg(errorParams[i]);

            return errorMessage;
        }

        QString toString( int val, const QList<QString>& errorParams )
        {
            return toString( static_cast<Value>(val), errorParams );
        }
    }
}
