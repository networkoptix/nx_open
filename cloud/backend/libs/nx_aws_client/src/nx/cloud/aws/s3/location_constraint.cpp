#include "location_constraint.h"

#include <functional>
#include <vector>

#include <QRegularExpression>

namespace nx::cloud::aws {

namespace {

static constexpr char kDefaultRegion[] = "us-east-1";

static constexpr char kDefaultLocationTemplate[] = R"xml(
    <LocationConstraint xmlns="http://s3.amazonaws.com/doc/2006-03-01/"/>
)xml";

static constexpr char kLocationTemplate[] = R"xml(
    <?xml version = "1.0" encoding = "UTF-8"?>
    <LocationConstraint xmlns="http://s3.amazonaws.com/doc/2006-03-01/">%1</LocationConstraint>
)xml";

static constexpr char kLocationConstraint[] = "LocationConstraint";
static constexpr char kRegion[] = "Region";

std::string getTagValue(const QString& tag, const QByteArray& messageBody)
{
    QRegularExpression regex("<" + tag + ".*>(.*)</" + tag + ">");
    return regex.match(messageBody).captured(1).toStdString();
}

static std::string parseLocationConstraintResponse(const QByteArray& messageBody)
{
    auto value = getTagValue(kLocationConstraint, messageBody);
    if (value.empty() && messageBody.contains(kLocationConstraint))
        return kDefaultRegion;
    return value;
}

static std::string parseWrongRegionResponse(const QByteArray& messageBody)
{
    return getTagValue(kRegion, messageBody);
}

static std::vector<std::function<std::string(const QByteArray&)>> kLocationXmlParsers = {
    parseWrongRegionResponse,
    parseLocationConstraintResponse
};

// TODO figure out xml deserialization AND how to deal with
// aws authorization catch 22 for getLocation api call
static std::string parseLocation(const QByteArray& messageBody)
{
    if (messageBody.isEmpty())
        return {};

    for (auto& func : kLocationXmlParsers)
    {
        auto region = func(messageBody);
        if (!region.empty())
            return region;
    }
    return {};
}

} // namespace

namespace xml {

bool deserialize(const QByteArray& data, s3::LocationConstraint* outObject)
{
    outObject->region = parseLocation(data);
    if (outObject->region.empty())
        return false;
    return true;
}

QByteArray serialized(const s3::LocationConstraint& object)
{
    if (object.region == kDefaultRegion)
        return kDefaultLocationTemplate;

    return QString(kLocationTemplate).arg(object.region.c_str()).toUtf8();
}

} // namespace xml

} // namespace nx::cloud::aws
