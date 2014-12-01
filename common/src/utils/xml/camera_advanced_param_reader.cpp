#include "camera_advanced_param_reader.h"

#include <QtXml/QDomElement>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/QXmlSchemaValidator>
#include <QtXmlPatterns/QAbstractMessageHandler>

#include <core/resource/resource.h>
#include <core/resource/param.h>

#include <utils/common/log.h>
#include <utils/common/model_functions.h>

namespace {
    const QUrl validatorSchemaUrl = lit("qrc:/camera_advanced_params/schema.xsd");

    //!Prints XML validation errors to log
    class QnXMLValidationMessageHandler: public QAbstractMessageHandler {
    public:
        //!Implementation of QAbstractMessageHandler::handleMessage
        virtual void handleMessage(
            QtMsgType type,
            const QString& description,
            const QUrl& identifier,
            const QSourceLocation& sourceLocation ) override { 
                if( type >= QtCriticalMsg ) {
                    NX_LOG( lit("Camera parameters XML validation error. Identifier: %1. Description: %2. Location: %3:%4").
                        arg(identifier.toString()).arg(description).arg(sourceLocation.line()).arg(sourceLocation.column()), cl_logDEBUG1 );
                }
        }
    };

    bool parseBooleanXmlValue(const QString &value) {
        return (value == lit("true")) || (value == lit("1"));
    }

    class QnIODeviceRAAI {
    public:
        QnIODeviceRAAI(QIODevice *device):
            m_device(device)
        {
                m_valid = device->open(QIODevice::ReadOnly);
        }

        ~QnIODeviceRAAI() {
            if (m_valid)
                m_device->close();
        }

        bool isValid() const {
            return m_valid;
        }
    private:
        QIODevice* m_device;
        bool m_valid;
    };
}

QnCameraAdvancedParamsReader::QnCameraAdvancedParamsReader() {}

QnCameraAdvancedParams QnCameraAdvancedParamsReader::params(const QnResourcePtr &resource) const {
	Q_ASSERT(resource);
    QnUuid id = resource->getId();

	/* Check if we have already read parameters for this camera. */
    if (!m_paramsByCameraId.contains(id)) {
        QnCameraAdvancedParams result = paramsFromResource(resource);
        m_paramsByCameraId[id] = result;
    }
	return m_paramsByCameraId[id];
}

void QnCameraAdvancedParamsReader::setParams(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) const {
    setParamsToResource(resource, params);
    m_paramsByCameraId[resource->getId()] = params;
}

QnCameraAdvancedParams QnCameraAdvancedParamsReader::paramsFromResource(const QnResourcePtr &resource) {
    Q_ASSERT(resource);
    QByteArray serialized = encodedParamsFromResource(resource).toUtf8();
    if (serialized.isEmpty())
        return QnCameraAdvancedParams();
    return QJson::deserialized<QnCameraAdvancedParams>(serialized);
}

void QnCameraAdvancedParamsReader::setParamsToResource(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) {
    Q_ASSERT(resource);
    QByteArray serialized = QJson::serialized(params);
    setEncodedParamsToResource(resource, QString::fromUtf8(serialized));
}

QString QnCameraAdvancedParamsReader::encodedParamsFromResource(const QnResourcePtr &resource) {
    Q_ASSERT(resource);
    return resource->getProperty(Qn::CAMERA_ADVANCED_PARAMETERS);
}

void QnCameraAdvancedParamsReader::setEncodedParamsToResource(const QnResourcePtr &resource, const QString &params) {
    Q_ASSERT(resource);
    resource->setProperty(Qn::CAMERA_ADVANCED_PARAMETERS, params);
}

bool QnCameraAdvacedParamsXmlParser::validateXml(QIODevice *xmlSource) {
    QnIODeviceRAAI guard(xmlSource);
    if (!guard.isValid())
        return false;

    QXmlSchema schema;
    if (!schema.load(validatorSchemaUrl))
        return false;
    Q_ASSERT(schema.isValid());
    QXmlSchemaValidator validator(schema);
    QnXMLValidationMessageHandler msgHandler;
    validator.setMessageHandler(&msgHandler);
    return validator.validate(xmlSource);
}

namespace QnXmlTag {
    const QString plugin		    = lit("plugin");
    const QString pluginName	    = lit("name");
    const QString pluginVersion	    = lit("version");
    const QString pluginUniqueId    = lit("unique_id");
    const QString pluginPacketMode  = lit("packet_mode");
    const QString parameters        = lit("parameters");
    const QString group				= lit("group");
    const QString param				= lit("param");
    const QString groupName 		= lit("name");
    const QString groupDescription	= lit("description");
    const QString paramId			= lit("id");
    const QString paramDataType		= lit("dataType");
    const QString paramName 		= lit("name");
    const QString paramDescription	= lit("description");
    const QString paramRange		= lit("range");
    const QString paramTag			= lit("tag");
    const QString paramReadOnly		= lit("readOnly");
}

bool QnCameraAdvacedParamsXmlParser::readXml(QIODevice *xmlSource, QnCameraAdvancedParams &result) {

    QnIODeviceRAAI guard(xmlSource);
    if (!guard.isValid())
        return false;

	QDomDocument xmlDom;
	{
		QString errorStr;
		int errorLine;
		int errorColumn;
		if (!xmlDom.setContent(xmlSource, &errorStr, &errorLine, &errorColumn)) {
			qWarning() << "Parse xml error at line: " << errorLine << ", column: " << errorColumn << ", error: " << errorStr;
			return false;
		}
	}

	QDomElement root = xmlDom.documentElement();
	if (root.tagName() != QnXmlTag::plugin) {
		qWarning() << "Parse xml error: could not find tag" << QnXmlTag::plugin;
		return false;
	}

    result.name         = root.attribute(QnXmlTag::pluginName);
    result.version      = root.attribute(QnXmlTag::pluginVersion);
    result.unique_id    = root.attribute(QnXmlTag::pluginUniqueId);
    result.packet_mode  = parseBooleanXmlValue(root.attribute(QnXmlTag::pluginPacketMode, lit("true")));
	
	return parsePluginXml(root, result);
}

bool QnCameraAdvacedParamsXmlParser::parsePluginXml(const QDomElement& pluginXml, QnCameraAdvancedParams &params) {
    for (QDomNode node = pluginXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (node.nodeName() != QnXmlTag::parameters) 
			continue;
	
        for (QDomNode groupNode = node.toElement().firstChild(); !groupNode.isNull(); groupNode = groupNode.nextSibling()) {
		    QnCameraAdvancedParamGroup group;
            if (!parseGroupXml(groupNode.toElement(), group))
                return false;
		    params.groups.push_back(group);
        }
	}
	return true;
}

bool QnCameraAdvacedParamsXmlParser::parseGroupXml(const QDomElement &groupXml, QnCameraAdvancedParamGroup &group) {

	group.name = groupXml.attribute(QnXmlTag::groupName);
	group.description = groupXml.attribute(QnXmlTag::groupDescription);

	for (QDomNode node = groupXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (node.nodeName() == QnXmlTag::group) {
			QnCameraAdvancedParamGroup subGroup;
            if (!parseGroupXml(node.toElement(), subGroup))
                return false;
			group.groups.push_back(subGroup);
		} else if (node.nodeName() == QnXmlTag::param) {
			QnCameraAdvancedParameter param;
            if (!parseElementXml(node.toElement(), param))
                return false;
			group.params.push_back(param);
		}
	}
	return true;
}

bool QnCameraAdvacedParamsXmlParser::parseElementXml(const QDomElement &elementXml, QnCameraAdvancedParameter &param) {
    param.id            = elementXml.attribute(QnXmlTag::paramId);
    param.dataType      = QnCameraAdvancedParameter::stringToDataType(elementXml.attribute(QnXmlTag::paramDataType));
	param.name          = elementXml.attribute(QnXmlTag::paramName);
	param.description   = elementXml.attribute(QnXmlTag::paramDescription);
	param.range         = elementXml.attribute(QnXmlTag::paramRange);
	param.tag           = elementXml.attribute(QnXmlTag::paramTag);
	param.readOnly      = parseBooleanXmlValue(elementXml.attribute(QnXmlTag::paramReadOnly));
	return true;
}
