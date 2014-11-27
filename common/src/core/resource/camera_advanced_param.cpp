#include "camera_advanced_param.h"

#include <QtXml/QDomElement>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/QXmlSchemaValidator>
#include <QtXmlPatterns/QAbstractMessageHandler>

#include <boost/range.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <core/resource/resource.h>
#include <core/resource/param.h>

#include <utils/common/log.h>
#include <utils/common/model_functions.h>

namespace {
	const QString defaultXmlRoot = lit(":/camera_advanced_params/");
    const QUrl validatorSchemaUrl = lit(":/camera_advanced_params/schema.xsd");

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
}

namespace QnXmlTag {
	const QString cameraType		= lit("camera");
	const QString cameraTypeName	= lit("name");
	const QString cameraTypeParent	= lit("parent");
	const QString group				= lit("group");
	const QString param				= lit("param");
	const QString name 				= lit("name");
	const QString description		= lit("description");
	const QString query				= lit("query");
	const QString dataType			= lit("dataType");
	const QString method			= lit("method");
	const QString min				= lit("min");
	const QString max				= lit("max");
	const QString step				= lit("step");
	const QString readOnly			= lit("readOnly");
}

QnCameraAdvancedParamValue::QnCameraAdvancedParamValue() {}

QnCameraAdvancedParamValue::QnCameraAdvancedParamValue(const QString &id, const QString &value):
	id(id), value(value)
{}

QnCameraAdvancedParamValueMap::QnCameraAdvancedParamValueMap():
    QMap<QString, QString>() {}

QnCameraAdvancedParamValueList QnCameraAdvancedParamValueMap::toValueList() const {
    QnCameraAdvancedParamValueList result;
    for (auto iter = this->cbegin(); iter != this->cend(); ++iter)
        result << QnCameraAdvancedParamValue(iter.key(), iter.value());
    return result;
}

QnCameraAdvancedParamValueList QnCameraAdvancedParamValueMap::difference(const QnCameraAdvancedParamValueMap &other) const {
    QnCameraAdvancedParamValueList result;
    for (auto iter = this->cbegin(); iter != this->cend(); ++iter) {
        if (other.contains(iter.key()) && other[iter.key()] == iter.value())
            continue;
        result << QnCameraAdvancedParamValue(iter.key(), iter.value());
    }
    return result;
}

void QnCameraAdvancedParamValueMap::appendValueList(const QnCameraAdvancedParamValueList &list) {
    for (const QnCameraAdvancedParamValue &value: list)
        insert(value.id, value.value);
}

QnCameraAdvancedParameter::QnCameraAdvancedParameter():
    dataType(DataType::None),
    readOnly(false)
{

}

QString QnCameraAdvancedParameter::getId() const {
	return query;
}

bool QnCameraAdvancedParameter::isValid() const {
	return (dataType != DataType::None) 
        && (!getId().isEmpty());
}

QString QnCameraAdvancedParameter::dataTypeToString(DataType value) {
	switch (value) {
	case QnCameraAdvancedParameter::DataType::Bool:
		return lit("Bool");
	case QnCameraAdvancedParameter::DataType::MinMaxStep:
		return lit("MinMaxStep");
	case QnCameraAdvancedParameter::DataType::Enumeration:
		return lit("Enumeration");
	case QnCameraAdvancedParameter::DataType::Button:
		return lit("Button");
	case QnCameraAdvancedParameter::DataType::String:
		return lit("String");
	}
	return QString();
}

QnCameraAdvancedParameter::DataType QnCameraAdvancedParameter::stringToDataType(const QString &value) {
	static QList<QnCameraAdvancedParameter::DataType> allDataTypes;
	if (allDataTypes.isEmpty())
		allDataTypes 
		<< DataType::Bool 
		<< DataType::MinMaxStep
		<< DataType::Enumeration
		<< DataType::Button
		<< DataType::String;
	for (auto dataType: allDataTypes)
		if (dataTypeToString(dataType) == value)
			return dataType;
	return DataType::None;
}

bool QnCameraAdvancedParameter::dataTypeHasValue(DataType value) {
	switch (value) {
	case QnCameraAdvancedParameter::DataType::Bool:
	case QnCameraAdvancedParameter::DataType::MinMaxStep:
	case QnCameraAdvancedParameter::DataType::Enumeration:
	case QnCameraAdvancedParameter::DataType::String:
		return true;
	case QnCameraAdvancedParameter::DataType::Button:
		return false;
	}
	return false;
}

QnCameraAdvancedParameter QnCameraAdvancedParamGroup::getParameterById(const QString &id) const {

    for (const QnCameraAdvancedParameter &param: params)
        if (param.getId() == id)
            return param;

    QnCameraAdvancedParameter result;
    for (const QnCameraAdvancedParamGroup &group: groups) {
        result = group.getParameterById(id);
        if (result.isValid())
            return result;
    }
    return result;
}

QSet<QString> QnCameraAdvancedParamGroup::allParameterIds() const {
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup &subGroup: groups)
        result.unite(subGroup.allParameterIds());
    for (const QnCameraAdvancedParameter &param: params)
        if (!param.getId().isEmpty())
            result.insert(param.getId());
    return result;
}

QnCameraAdvancedParameter QnCameraAdvancedParams::getParameterById(const QString &id) const {
    QnCameraAdvancedParameter result;
    for (const QnCameraAdvancedParamGroup &group: groups) {
        result = group.getParameterById(id);
        if (result.isValid())
            return result;
    }
    return result;
}

QSet<QString> QnCameraAdvancedParams::allParameterIds() const {
    QSet<QString> result;
    for (const QnCameraAdvancedParamGroup &group: groups)
        result.unite(group.allParameterIds());
    return result;
}

QnCameraAdvancedParamsReader::QnCameraAdvancedParamsReader() {
    QString mask = lit("*.xml");
    QDir defaultXmlRootDir(defaultXmlRoot);
    for(const QString &fileName: defaultXmlRootDir.entryList(QStringList(mask), QDir::Files| QDir::Readable)) {
        QFile dataSource(defaultXmlRootDir.absoluteFilePath(fileName));

#ifdef _DEBUG //validating xml
        Q_ASSERT(QnCameraAdvacedParamsXmlParser::validateXml(&dataSource));
#endif // _DEBUG

        QnCameraAdvancedParams params;
        QString cameraType;
        if (QnCameraAdvacedParamsXmlParser::readXml(&dataSource, params, cameraType))
            m_paramsByCameraType[cameraType] = params;
    }
}

QnCameraAdvancedParams QnCameraAdvancedParamsReader::params(const QnResourcePtr &resource) const {
	Q_ASSERT(resource);

	QString cameraType = calculateCameraType(resource);
    if (cameraType.isEmpty())
        return QnCameraAdvancedParams();

	/* Check if we have already read parameters for this camera. */
	if (m_paramsByCameraType.contains(cameraType))
		return m_paramsByCameraType[cameraType];

	/* Check if the camera has its own xml. */
	QByteArray cameraSettingsXml = resource->getProperty(Qn::CAMERA_ADVANCED_PARAMS_XML).toUtf8();
    if (cameraSettingsXml.isEmpty())
        return QnCameraAdvancedParams();

	/* If yes, read it. */
    QBuffer dataSource(&cameraSettingsXml);
    QnCameraAdvancedParams result;
    if (QnCameraAdvacedParamsXmlParser::readXml(&dataSource, result, cameraType)) {
        /* Cache result for future use in case of success. */
        m_paramsByCameraType[cameraType] = result;
    }
	return result;
}

QString QnCameraAdvancedParamsReader::calculateCameraType(const QnResourcePtr &resource) const {
	return resource->getProperty(Qn::CAMERA_ADVANCED_PARAMS_TYPENAME);
}

QSet<QString> QnCameraAdvancedParamsReader::allowedParams(const QnResourcePtr &resource) {
    return resource->getProperty(Qn::CAMERA_ADVANCED_PARAMS_SUPPORTED).split(L',', QString::SkipEmptyParts).toSet();
}

bool QnCameraAdvacedParamsXmlParser::validateXml(QIODevice *xmlSource) {
    QXmlSchema schema;
    if (!schema.load(validatorSchemaUrl))
        return false;
    Q_ASSERT(schema.isValid());
    QXmlSchemaValidator validator(schema);
    QnXMLValidationMessageHandler msgHandler;
    validator.setMessageHandler(&msgHandler);
    return validator.validate(xmlSource);
}

bool QnCameraAdvacedParamsXmlParser::readXml(QIODevice *xmlSource, QnCameraAdvancedParams &result) {

	QDomDocument xmlDom;
	{
		QString errorStr;
		int errorLine;
		int errorColumn;
		if (!xmlDom.setContent(xmlSource, &errorStr, &errorLine, &errorColumn))
		{
			qWarning() << "Parse xml error at line: " << errorLine << ", column: " << errorColumn << ", error: " << errorStr;
			return false;
		}
	}

	QDomElement root = xmlDom.documentElement();
	if (root.tagName() != QnXmlTag::cameraType) {
		qWarning() << "Parse xml error: could not find 'camera' tag";
		return false;
	}

    result.name         = root.attributes().namedItem(lit("name")).nodeValue();
    result.version      = root.attributes().namedItem(lit("version")).nodeValue();
    result.unique_id    = root.attributes().namedItem(lit("unique_id")).nodeValue();
	
	return parseCameraXml(root, result);
}

bool QnCameraAdvacedParamsXmlParser::parseCameraXml(const QDomElement& cameraXml, QnCameraAdvancedParams &params) {
	for (QDomNode node = cameraXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (node.nodeName() != QnXmlTag::group) 
			continue;
		
		QnCameraAdvancedParamGroup group;
        if (!parseGroupXml(node.toElement(), group))
            return false;
		params.groups.push_back(group);
	}
	return true;
}

bool QnCameraAdvacedParamsXmlParser::parseGroupXml(const QDomElement &groupXml, QnCameraAdvancedParamGroup &group) {

	group.name = groupXml.attribute(QnXmlTag::name);
	group.description = groupXml.attribute(QnXmlTag::description);

	for (QDomNode node = groupXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
		if (node.nodeName() == QnXmlTag::group) {
			QnCameraAdvancedParamGroup group;
            if (!parseGroupXml(node.toElement(), group))
                return false;
			group.groups.push_back(group);
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
	param.name = elementXml.attribute(QnXmlTag::name);
	param.description = elementXml.attribute(QnXmlTag::description);
	param.query = elementXml.attribute(QnXmlTag::query);
	param.dataType = QnCameraAdvancedParameter::stringToDataType(elementXml.attribute(QnXmlTag::dataType));
	param.method = elementXml.attribute(QnXmlTag::method, lit("GET"));
	param.min = elementXml.attribute(QnXmlTag::min);
	param.max = elementXml.attribute(QnXmlTag::max);
	param.step = elementXml.attribute(QnXmlTag::step);
	param.readOnly = elementXml.attribute(QnXmlTag::readOnly) == lit("true");

	return true;
}


bool deserialize(QnJsonContext *, const QJsonValue &value, QnCameraAdvancedParameter::DataType *target) {
	*target = QnCameraAdvancedParameter::stringToDataType(value.toString());
	return true;
}

void serialize(QnJsonContext *ctx, const QnCameraAdvancedParameter::DataType &value, QJsonValue *target) {
	QJson::serialize(ctx, QnCameraAdvancedParameter::dataTypeToString(value), target);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
	QnCameraAdvancedParameterTypes,
	(json),
	_Fields,
	(optional, true)
	)
