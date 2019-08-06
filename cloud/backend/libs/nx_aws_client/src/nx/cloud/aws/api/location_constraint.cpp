#include "location_constraint.h"

#include <vector>
#include <functional>

namespace nx::cloud::aws::api::xml {

namespace {

static std::string parseLocationConstraintResponse(const QByteArray& messageBody)
{
    /**
     * Parses aws-region-x between tags <LocationConstraint>aws-region-x</LocationRestraint>
     * if </LocationConstraint> end tag is missing, the region is assumed to be us-east-1 per
     * AWS docs: https://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGETlocation.html
     */

    static constexpr char kLocationConstraint[] = "LocationConstraint";
    if (messageBody.indexOf(kLocationConstraint) == -1) //< Required xml tag is missing
        return {};

    static constexpr char kXmlEndTag[] = "</LocationConstraint>";
    static constexpr char kUsEast1[] = "us-east-1";

    int end = messageBody.indexOf(kXmlEndTag);
    if (end == -1)
        return kUsEast1;

    for (int i = end - 1; i >= 0; --i)
    {
        if (messageBody.at(i) == '>')
            return messageBody.mid(i + 1, end - i - 1).trimmed().toStdString();
    }

    return {};
}

static std::string parseWrongRegionResponse(const QByteArray& messageBody)
{
    // Parses 'aws-region-x' between tags: <Region>aws-region-x</Region>
    static QByteArray kRegionStartTag = "<Region>";
    static QByteArray kRegionEndTag = "</Region>";

    int start = messageBody.indexOf(kRegionStartTag);
    if (start == -1)
        return {};
    start += kRegionStartTag.size();

    int end = messageBody.indexOf(kRegionEndTag, start);
    if (end == -1)
        return {};

    if (end <= start)
        return {};

    return messageBody.mid(start, end - start).trimmed().toStdString();
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

bool deserialize(const QByteArray& data, LocationConstraint* outObject)
{
    auto region = parseLocation(data);
    if (region.empty())
        return false;
    outObject->region = region;
    return true;
}

} // namespace nx::cloud::aws::api::xml