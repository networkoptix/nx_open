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

bool QnCameraAdvancedParameter::isValid() const {
	return (dataType != DataType::None) 
        && (!id.isEmpty());
}

QString QnCameraAdvancedParameter::dataTypeToString(DataType value) {
	switch (value) {
	case DataType::Bool:
		return lit("Bool");
	case DataType::Number:
		return lit("Number");
	case DataType::Enumeration:
		return lit("Enumeration");
	case DataType::Button:
		return lit("Button");
	case DataType::String:
		return lit("String");
	}
	return QString();
}

QnCameraAdvancedParameter::DataType QnCameraAdvancedParameter::stringToDataType(const QString &value) {
	static QList<QnCameraAdvancedParameter::DataType> allDataTypes;
	if (allDataTypes.isEmpty())
		allDataTypes 
		<< DataType::Bool 
		<< DataType::Number
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
	case DataType::Bool:
	case DataType::Number:
	case DataType::Enumeration:
	case DataType::String:
		return true;
	case DataType::Button:
		return false;
	}
	return false;
}

QStringList QnCameraAdvancedParameter::getRange() const {
    Q_ASSERT(dataType == DataType::Enumeration);
    return range.split(L',', QString::SkipEmptyParts);
}

void QnCameraAdvancedParameter::getRange(double &min, double &max) const {
    Q_ASSERT(dataType == DataType::Number);
    QStringList values = range.split(L',');
    Q_ASSERT(values.size() == 2);
    if (values.size() != 2)
        return;
    min = values[0].toDouble();
    max = values[1].toDouble();
}


void QnCameraAdvancedParameter::setRange(int min, int max) {
    Q_ASSERT(dataType == DataType::Number);
    range = lit("%1,%2").arg(min).arg(max);
}

void QnCameraAdvancedParameter::setRange(double min, double max) {
    Q_ASSERT(dataType == DataType::Number);
    range = lit("%1,%2").arg(min).arg(max);
}

QnCameraAdvancedParameter QnCameraAdvancedParamGroup::getParameterById(const QString &id) const {
    for (const QnCameraAdvancedParameter &param: params)
        if (param.id == id)
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
        if (!param.id.isEmpty())
            result.insert(param.id);
    return result;
}

bool QnCameraAdvancedParamGroup::updateParameter(const QnCameraAdvancedParameter &parameter) {
    for (QnCameraAdvancedParameter &param: params) {
        if (param.id == parameter.id) {
            param = parameter;
            return true;
        }
    }
    for (QnCameraAdvancedParamGroup &group: groups) {
        if (group.updateParameter(parameter))
            return true;
    }
    return false;
}

QnCameraAdvancedParamGroup QnCameraAdvancedParamGroup::filtered(const QSet<QString> &allowedIds) const {
    QnCameraAdvancedParamGroup result;
    for (const QnCameraAdvancedParamGroup &subGroup: groups) {
        QnCameraAdvancedParamGroup group = subGroup.filtered(allowedIds);
        if (!group.isEmpty())
            result.groups.push_back(group);
    }
    for (const QnCameraAdvancedParameter &param: params) {
        if (param.isValid() && allowedIds.contains(param.id))
            result.params.push_back(param);
    }
    return result;
}

bool QnCameraAdvancedParamGroup::isEmpty() const {
    return params.empty() && groups.empty();
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

bool QnCameraAdvancedParams::updateParameter(const QnCameraAdvancedParameter &parameter) {
    for (QnCameraAdvancedParamGroup &group: groups) {
        if (group.updateParameter(parameter))
            return true;
    }
    return false;
}

void QnCameraAdvancedParams::clear() {
    groups.clear();
}

QnCameraAdvancedParams QnCameraAdvancedParams::filtered(const QSet<QString> &allowedIds) const {
    QnCameraAdvancedParams result;
    result.name = this->name;
    result.unique_id = this->unique_id;
    result.version = this->version;
    for (const QnCameraAdvancedParamGroup &subGroup: groups) {
        QnCameraAdvancedParamGroup group = subGroup.filtered(allowedIds);
        if (!group.isEmpty())
            result.groups.push_back(group);
    }
    return result;
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
    QByteArray serialized = resource->getProperty(Qn::CAMERA_ADVANCED_PARAMETERS).toUtf8();
    if (serialized.isEmpty())
        return QnCameraAdvancedParams();
    return QJson::deserialized<QnCameraAdvancedParams>(serialized);
}

void QnCameraAdvancedParamsReader::setParamsToResource(const QnResourcePtr &resource, const QnCameraAdvancedParams &params) {
    Q_ASSERT(resource);
    QByteArray serialized = QJson::serialized(params);
    resource->setProperty(Qn::CAMERA_ADVANCED_PARAMETERS, QString::fromUtf8(serialized));
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

namespace QnXmlTag {
    const QString plugin		    = lit("plugin");
    const QString pluginName	    = lit("name");
    const QString pluginVersion	    = lit("version");
    const QString pluginUniqueId    = lit("unique_id");
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

    result.name         = root.attributes().namedItem(QnXmlTag::pluginName).nodeValue();
    result.version      = root.attributes().namedItem(QnXmlTag::pluginVersion).nodeValue();
    result.unique_id    = root.attributes().namedItem(QnXmlTag::pluginUniqueId).nodeValue();
	
	return parsePluginXml(root, result);
}

bool QnCameraAdvacedParamsXmlParser::parsePluginXml(const QDomElement& pluginXml, QnCameraAdvancedParams &params) {
    for (QDomNode node = pluginXml.firstChild(); !node.isNull(); node = node.nextSibling()) {
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

	group.name = groupXml.attribute(QnXmlTag::groupName);
	group.description = groupXml.attribute(QnXmlTag::groupDescription);

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
    param.id            = elementXml.attribute(QnXmlTag::paramId);
    param.dataType      = QnCameraAdvancedParameter::stringToDataType(elementXml.attribute(QnXmlTag::paramDataType));
	param.name          = elementXml.attribute(QnXmlTag::paramName);
	param.description   = elementXml.attribute(QnXmlTag::paramDescription);
	param.range         = elementXml.attribute(QnXmlTag::paramRange);
	param.tag           = elementXml.attribute(QnXmlTag::paramTag);
	param.readOnly      = elementXml.attribute(QnXmlTag::paramReadOnly) == lit("true");
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
