/**********************************************************
* 27 oct 2014
* akolesnikov
***********************************************************/

#ifndef CAMERA_PARAMS_DESCRIPTION_XML_HELPER_H
#define CAMERA_PARAMS_DESCRIPTION_XML_HELPER_H

#include <list>
#include <map>

#include <QtXmlPatterns/QAbstractMessageHandler>
#include <QtXml/QXmlDefaultHandler>

#include <plugins/resource/camera_settings/camera_settings.h>
#include <utils/common/log.h>


//!Prints XML validation errors to log
class ParamsXMLValidationMessageHandler
:
    public QAbstractMessageHandler
{
public:
    //!Implementation of QAbstractMessageHandler::handleMessage
    virtual void handleMessage(
        QtMsgType type,
        const QString& description,
        const QUrl& identifier,
        const QSourceLocation& sourceLocation ) override;
};

class CameraParamsReader
:
    public CameraSettingReader
{
public:
    CameraParamsReader(
        std::map<QString, CameraSetting>* const cameraSettings,
        const QnResourcePtr& cameraRes );

    const std::list<QString>& foundCameraNames() const;

protected:
    virtual bool isGroupEnabled(const QString& id, const QString& parentId, const QString& name) override;
    virtual bool isParamEnabled(const QString& id, const QString& parentId) override;
    virtual void paramFound(const CameraSetting& value, const QString& parentId) override;
    virtual void cleanDataOnFail() override;
    virtual void parentOfRootElemFound(const QString& parentId) override;
    virtual void cameraElementFound(const QString& cameraName, const QString& parentId) override;

private:
    std::map<QString, CameraSetting>* const m_cameraSettings;
    std::list<QString> m_foundCameraNames;
};

class CameraParamsHandler
:
    public QXmlDefaultHandler
{
public:
    virtual bool startDocument();
    virtual bool startElement( const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts );
    virtual bool endDocument();
    virtual bool endElement( const QString& namespaceURI, const QString& localName, const QString& qName );
    virtual QString errorString() const;
    virtual bool error( const QXmlParseException& exception );
    virtual bool fatalError( const QXmlParseException& exception );

    QString cameraID() const;

private:
    QString m_cameraID;
    QString m_errorDescription;
};

#endif  //CAMERA_PARAMS_DESCRIPTION_XML_HELPER_H
