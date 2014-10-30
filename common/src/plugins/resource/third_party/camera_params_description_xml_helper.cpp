/**********************************************************
* 27 oct 2014
* akolesnikov
***********************************************************/

#include "camera_params_description_xml_helper.h"


void ParamsXMLValidationMessageHandler::handleMessage(
    QtMsgType type,
    const QString& description,
    const QUrl& identifier,
    const QSourceLocation& sourceLocation )
{
    if( type >= QtCriticalMsg )
    {
        NX_LOG( lit("Camera parameters XML validation error. Identifier: %1. Description: %2. Location: %3:%4").
            arg(identifier.toString()).arg(description).arg(sourceLocation.line()).arg(sourceLocation.column()), cl_logDEBUG1 );
    }
}


CameraParamsReader::CameraParamsReader(
    std::map<QString, CameraSetting>* const cameraSettings,
    const QnResourcePtr& cameraRes )
:
    CameraSettingReader( QString(), cameraRes ),
    m_cameraSettings( cameraSettings )
{
}

const std::list<QString>& CameraParamsReader::foundCameraNames() const
{
    return m_foundCameraNames;
}

bool CameraParamsReader::isGroupEnabled(const QString& id, const QString& /*parentId*/, const QString& /*name*/)
{
    CameraSetting setting;
    setting.setType( CameraSetting::Group );
    (*m_cameraSettings)[id] = setting;
    return true;
}

bool CameraParamsReader::isParamEnabled(const QString& /*id*/, const QString& /*parentId*/)
{
    return true;
}

void CameraParamsReader::paramFound(const CameraSetting& value, const QString& /*parentId*/)
{
    //QString id = value.getId();
    //QHash<QString, QSharedPointer<OnvifCameraSettingOperationAbstract> >::ConstIterator it;

    CameraSetting currentSetting;
    switch(value.getType())
    {
        case CameraSetting::OnOff:
        case CameraSetting::MinMaxStep:
        case CameraSetting::Enumeration:
        case CameraSetting::TextField:
        case CameraSetting::ControlButtonsPair:
            (*m_cameraSettings)[value.getId()] = value;
            //it = OnvifCameraSettingOperationAbstract::operations.find(id);
            //if (it == OnvifCameraSettingOperationAbstract::operations.end()) {
            //    //Operations must be defined for all settings
            //    Q_ASSERT(false);
            //}
            //it.value()->get(currentSetting, m_settings);
            //m_settings.getCameraSettings().insert(id, 
            //    OnvifCameraSetting(*(it.value()), id, value.getName(),
            //    value.getType(), value.getQuery(), value.getDescription(), 
            //    value.getMin(), value.getMax(), value.getStep(), currentSetting.getCurrent()));
            return;

        case CameraSetting::Button:
            //If absent = enabled
            return;

        default:
            //XML must contain only valid types
            Q_ASSERT(false);
            return;
    }
}

void CameraParamsReader::cleanDataOnFail()
{
}

void CameraParamsReader::parentOfRootElemFound(const QString& /*parentId*/)
{
}

void CameraParamsReader::cameraElementFound(const QString& cameraName, const QString& /*parentId*/)
{
    m_foundCameraNames.push_back( cameraName );
}


bool CameraParamsHandler::startDocument()
{
    return true;
}

bool CameraParamsHandler::startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts )
{
    if( qName == lit("camera") )
    {
        int nameAttrPos = atts.index(lit("name"));
        if( nameAttrPos == -1 )
            return false;

        m_cameraID = atts.value(nameAttrPos);
    }

    return true;
}

bool CameraParamsHandler::endDocument()
{
    return true;
}

bool CameraParamsHandler::endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
{
    return true;
}

QString CameraParamsHandler::errorString() const
{
    return m_errorDescription;
}

bool CameraParamsHandler::error( const QXmlParseException& exception )
{
    m_errorDescription = lit("Parse error. line %1, col %2, parser message: %3").
        arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

bool CameraParamsHandler::fatalError( const QXmlParseException& exception )
{
    m_errorDescription = lit("Fatal parse error. line %1, col %2, parser message: %3").
        arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

QString CameraParamsHandler::cameraID() const
{
    return m_cameraID;
}
