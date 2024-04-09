// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/rapidjson/processor.h>
#include <nx/reflect/instrument.h>

#include <nx/utils/string.h>

namespace {

const QString json = R"(
    { "sections":
        [
            { "name": "A" },
            {
                "name": "B",
                "sections":
                [
                    { "name": "C" },
                    { "name": "D", "sections": [{ "name": "E" } ] } ,
                    { "name": "F" },
                    { "name": "G" }
                ]
            } ,
            { "name": "H", "sections": [{ "name": "I" } ]}
        ]
    })";

const QString smallJson = R"(
{
    "name": "smallObject",
    "sections": [{"name": "S1"}, {"name": "S2"}]
})";

void insertAfterPeviousElement(
    QString& jsonString, const QString& peviousElement, const QString& newElement)
{
    auto ind = jsonString.indexOf(peviousElement);
    jsonString.insert(ind + peviousElement.size(), newElement);
}

std::string documentToString(const rapidjson::Document& doc)
{
    ::rapidjson::StringBuffer buffer;
    ::rapidjson::Writer<::rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

using Comp = nx::utils::rapidjson::predicate::Compare;
using Cont = nx::utils::rapidjson::predicate::ContainedIn;
using All = nx::utils::rapidjson::predicate::All;
using NameCont = nx::utils::rapidjson::predicate::NameContains;
using HasMember = nx::utils::rapidjson::predicate::HasMember;

struct SmallObject
{
    QString name;
    std::vector<SmallObject> sections;

    bool operator==(const SmallObject& right) const = default;
};

struct SimplestElement
{
    std::string name;
};

struct Coordinate
{
    std::string extrapolationMode;
    std::vector<double> device;
    std::vector<double> logical;
    double logicalMultiplier = 0;
    std::string space;
};

NX_REFLECTION_INSTRUMENT(SmallObject, (name) (sections));
NX_REFLECTION_INSTRUMENT(SimplestElement, (name));
NX_REFLECTION_INSTRUMENT(
    Coordinate, (extrapolationMode) (device) (logical) (logicalMultiplier) (space));

const QString manyFieldsObject = R"(
{
    "name": "manyFieldsObject",
    "A1": "1",
    "A2": "2",
    "B1": "3",
    "A3": "4",
    "B2": "5",
    "A4": "6",
    "A5": "7",
    "B3": "8",
    "object1": {"A11": "11", "A12": "12", "B11": "13"},
    "object2": {"A21": "21", "B21": "22", "A22": "23"}
}
)";

const QString coordinate = R"(
{
    "y":
    {
        "extrapolationMode": "ConstantExtrapolation",
        "device":  [ -1,   1],
        "logical": [  20, -90]
    },
    "z":
    {
        "extrapolationMode": "ConstantExtrapolation",
        "device":  [0, 0.05,  0.1, 0.15,  0.2, 0.25,  0.3, 0.35,  0.4, 0.45,  0.5, 0.55, 0.6, 0.65,  0.7, 0.75,  0.8,  0.85,   0.9,  0.95,  1],
        "logical": [1, 1.11, 1.22, 1.36, 1.48, 1.64, 1.81, 2.03, 2.29, 2.63, 3.05, 3.49, 4.18, 5.05, 6.27, 7.93, 10.24, 13.22, 17.02, 21.97, 31.55],
        "logicalMultiplier": 32.0,
        "space": "35MmEquiv"
    }
}
)";

} // namespace

namespace nx::utils::rapidjson::test {

TEST(Processor, IteratorHelperTest)
{
    auto pred =
        [](auto& val)
        {
            using T = std::decay_t<decltype(val)>;
            std::string name;
            if constexpr (std::is_same_v<T, ::rapidjson::Value::MemberIterator>)
                name = val->name.GetString();
            else
                name = val->GetString();
            return name.starts_with("A");
        };

    details::Condition cond{pred};

    auto pred2 =
        [](auto& val)
        {
            using T = std::decay_t<decltype(val)>;
            std::string name;
            if constexpr (std::is_same_v<T, ::rapidjson::Value::MemberIterator>)
                name = val->name.GetString();
            else
                name = val->GetString();
            return name.find("1") != std::string::npos;
        };

    details::Condition cond2{pred2};

    std::vector<std::string> arr{"A1", "A2", "B1", "A3", "B2", "A4", "B3", "B4", "A5"};
    std::string strVal = nx::reflect::json::serialize(arr);
    QString arrStr = QString::fromStdString(strVal);
    ::rapidjson::Document arrDoc;
    arrDoc.Parse(strVal);
    details::ArrayPtrHelper arrWr = &arrDoc;

    for (auto it = arrWr.begin(); it != arrWr.end();)
    {
        if (cond2(it))
        {
            ::rapidjson::Value v{"new_val"};
            arrWr.set(it, std::move(v));
            ASSERT_TRUE(v.IsNull());
        }
        ++it;
    }
    arrStr.replace("A1", "new_val");
    arrStr.replace("B1", "new_val");
    ASSERT_EQ(arrStr.toStdString(), documentToString(arrDoc));

    for (auto it = arrWr.begin(); it != arrWr.end();)
    {
        if (cond(it))
            it = arrWr.erase(it);
        else
            ++it;
    }

    QStringList elemToRemove{R"(,"A2")", R"(,"A3")", R"(,"A4")", R"(,"A5")"};
    for (auto el: elemToRemove)
        arrStr.remove(el);
    ASSERT_EQ(arrStr.toStdString(), documentToString(arrDoc));

    ::rapidjson::Document objDoc;
    objDoc.Parse(manyFieldsObject.toUtf8());
    QString objStr{manyFieldsObject.simplified().remove(" ")};
    details::ObjectPtrHelper objWr = &objDoc;

    for (auto it = objWr.begin(); it != objWr.end();)
    {
        if (cond2(it))
        {
            ::rapidjson::Value v{"new_val"};
            objWr.set(it, std::move(v));
            ASSERT_TRUE(v.IsNull());
        }
        ++it;
    }
    objStr.replace(R"("1")", R"("new_val")");
    objStr.replace(R"("3")", R"("new_val")");
    objStr.replace(R"({"A11":"11","A12":"12","B11":"13"})", R"("new_val")");
    ASSERT_EQ(objStr.toStdString(), documentToString(objDoc));

    for (auto it = objWr.begin(); it != objWr.end();)
    {
        if (cond(it))
            it = objWr.erase(it);
        else
            ++it;
    }
    objStr.remove(R"(,"A1":"new_val","A2":"2")");
    objStr.remove(R"(,"A3":"4")");
    objStr.remove(R"(,"A4":"6","A5":"7")");
    ASSERT_EQ(objStr.toStdString(), documentToString(objDoc));
}

TEST(Processor, ValueHelperTest)
{
    ::rapidjson::Document smallDoc;
    smallDoc.Parse(smallJson.toUtf8());
    ::rapidjson::Value* root = &smallDoc;

    details::ValueHelper val1{"newName", smallDoc.GetAllocator()};
    root->GetObject()["name"] = val1.get<details::ReturnCount::One>();
    ASSERT_TRUE(val1.isNull());
    ASSERT_EQ(std::string(root->GetObject()["name"].GetString()), std::string("newName"));

    details::ValueHelper val2{"newName2", smallDoc.GetAllocator()};
    root->GetObject()["name"] = val2.get<details::ReturnCount::All>();
    ASSERT_FALSE(val2.isNull());
    ASSERT_EQ(std::string(root->GetObject()["name"].GetString()), std::string("newName2"));
}

TEST(Processor, PathTest)
{
    All cond;
    details::Path tokens{"/sections/[?]/sections/[?]/sections/0/name", cond, cond};
    ASSERT_TRUE(tokens.isValid());
    ASSERT_EQ(tokens.tokenCount(), 5);
    ASSERT_EQ(tokens.conditionCount(), 2);

    details::Path invalidTokens{"sections/[?]/sections/[?]/sections/0/name", cond, cond};
    ASSERT_FALSE(invalidTokens.isValid());

    details::Path invalidTokens2{"/sections/[?]/sections/[?]/sections/0/name", cond};
    ASSERT_FALSE(invalidTokens2.isValid());

    details::Path invalidTokens3{"/sections/[?]/sections/[?]/sections/0/name", cond, cond, cond};
    ASSERT_FALSE(invalidTokens3.isValid());

    details::Path tokensSartsWithCondition{
        "/[?]/sections/[?]/sections/0/[?]/[?]/sections/0/name/[?]", cond, cond, cond, cond, cond};
    ASSERT_TRUE(tokensSartsWithCondition.isValid());
    ASSERT_EQ(tokensSartsWithCondition.tokenCount(), 8);
    ASSERT_EQ(tokensSartsWithCondition.conditionCount(), 5);

    details::Path simplePath{"/sections/0/name"};
    ASSERT_TRUE(simplePath.isValid());
    ASSERT_EQ(simplePath.tokenCount(), 1);
    ASSERT_EQ(simplePath.conditionCount(), 0);

    details::Path emptyPath{""};
    ASSERT_TRUE(emptyPath.isValid());
    ASSERT_EQ(emptyPath.tokenCount(), 1);
    ASSERT_EQ(emptyPath.conditionCount(), 0);
}

TEST(Processor, Find)
{
    // Positive cases.
    Processor r{json.toUtf8()};
    auto v1 = r.getValue<QString>("/sections/0/name");
    ASSERT_TRUE(v1.has_value());
    ASSERT_EQ(v1.value(), "A");
    auto v2 = r.getValue<QString>(
        "/sections/[?]/sections/[?]/sections/0/name",
        Comp("name", "B"),
        Comp("name", "D"));
    ASSERT_TRUE(v2.has_value());
    ASSERT_EQ(v2.value(), "E");

    auto v3 = r.getValue<::rapidjson::Value*>("/sections/[?]", Comp("name", "H"));
    ASSERT_TRUE(v3.has_value());
    std::string name{v3.value()->FindMember("name")->value.GetString()};
    ASSERT_EQ(name, "H");

    auto v4 = r.getAllValues<QString>(
        "/sections/[?]/sections/[?]/name",
        Comp("name", "B"),
        Cont("name", QStringList{"C", "F"}));
    ASSERT_EQ(v4.size(), 2);
    ASSERT_EQ(v4[0], "C");
    ASSERT_EQ(v4[1], "F");

    // More complex cases.
    auto v5 = r.getAllValues<QString>(
        "/sections/[?]/sections/[?]/name", Cont("name", QStringList{"B", "H"}), Comp("name", "C"));
    ASSERT_EQ(v5.size(), 1);
    ASSERT_EQ(v5[0], "C");

    // Negative cases.
    // Invalid name.
    auto v6 = r.getValue<QString>("/ssssections/0/name");
    ASSERT_FALSE(v6.has_value());
    // Condition for array applies to object.
    auto v7 = r.getValue<QString>("/sections/0/[?]", Comp("name", "C"));
    ASSERT_FALSE(v7.has_value());
    // Array has no object satisfying condition.
    auto v8 = r.getValue<QString>("/sections/[?]/name", Comp("name", "C"));
    ASSERT_FALSE(v8.has_value());

    auto v8a = r.getAllValues<QString>("/sections/[?]/name", Comp("name", "C"));
    ASSERT_TRUE(v8a.empty());
    // Invalid type of found value.
    auto v9 = r.getValue<QString>("/sections/[?]", Comp("name", "B"));
    ASSERT_FALSE(v9.has_value());
    // Wrong number of conditions.
    auto v10 = r.getValue<QString>("/sections/[?]/sections/[?]/name", Comp("name", "B"));
    ASSERT_FALSE(v10.has_value());
    // Invalid member name in condition.
    auto v11 = r.getValue<QString>("/sections/[?]/name", Comp("section", "B"));
    ASSERT_FALSE(v11.has_value());

    // Recursive search.
    std::vector<Processor> v12 = r.find(Cont("name", QStringList{"A", "E", "G", "I"}));
    std::vector<std::string> res;
    for (auto& v: v12)
        if (auto returned = v.getValue<std::string>("/name"); returned.has_value())
            res.push_back(returned.value());

    ASSERT_EQ(res.size(), 4);
    std::vector<std::string> rightResult{"A", "E", "G", "I"};
    ASSERT_EQ(res, rightResult);

    std::vector<Processor> v13 = r.find(Cont("name", QStringList{"D", "F", "H"}));
    res.clear();
    for (auto& v: v13)
        if (auto returned = v.getValue<std::string>("/sections/0/name"); returned.has_value())
            res.push_back(returned.value());
    ASSERT_EQ(v13.size(), 3);
    ASSERT_EQ(res.size(), 2);
    std::vector<std::string> rightResult2{"E", "I"};
    ASSERT_EQ(res, rightResult2);
    res.clear();
    for (auto& v: v13)
        if (auto returned = v.getValue<std::string>("/sections/[?]/name", Comp("name", "E")); returned.has_value())
            res.push_back(returned.value());
    ASSERT_EQ(res.size(), 1);
    ASSERT_EQ(res[0], "E");

    auto v15 = r.getValue<SmallObject>("/sections/1/sections/1");
    ASSERT_TRUE(v15.has_value());
    SmallObject rightResult3{.name="D", .sections={SmallObject{.name="E"}}};
    ASSERT_EQ(v15.value(), rightResult3);

    // Test search in objects.
    Processor rManyFields{manyFieldsObject.toUtf8()};
    auto v16 = rManyFields.getAllValues<std::string>("/[?]", NameCont("B"));
    std::vector<std::string> rightResult4{"3", "5", "8"};
    ASSERT_EQ(v16, rightResult4);

    auto v17 = rManyFields.getAllValues<std::string>("/[?]/[?]", All(), NameCont("1"));
    std::vector<std::string> rightResult5{"11", "12", "13", "21", "22"};
    ASSERT_EQ(v17, rightResult5);

    // Empty path.
    Processor rSmallObject{smallJson.toUtf8()};
    auto v18 = rSmallObject.getValue<SmallObject>("");
    SmallObject rightResult6{.name = "smallObject", .sections={SmallObject{.name="S1"}, SmallObject{.name="S2"}}};
    ASSERT_EQ(v18, rightResult6);

    // Return Processor.
    auto v19 = rSmallObject.getValue<Processor>("/sections");
    ASSERT_EQ(v19->toStdString(), R"([{"name":"S1"},{"name":"S2"}])");
    ASSERT_EQ(v19->getValue<std::string>("/0/name").value(), "S1");

    {
        Processor rCoordinate{coordinate.toUtf8()};
        auto v = rCoordinate.getAllValues<Coordinate>("/[?]", All());
        ASSERT_EQ(v.size(), 2);

        auto vDouble = rCoordinate.getValue<double>("/z/logical/1");
        ASSERT_TRUE(vDouble.has_value());
        ASSERT_EQ(vDouble.value(), 1.11);
    }
}

TEST(Processor, Erase)
{
    Processor r{json.toUtf8()};
    QString jsonString{json.simplified().remove(" ")};

    jsonString.remove(R"({"name":"A"},)");
    auto s = r.eraseValue("/sections/0");
    ASSERT_TRUE(s);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    jsonString.remove(R"(,"sections":[{"name":"E"}])");
    auto s2 = r.eraseValue(
        "/sections/[?]/sections/[?]/sections", Comp("name", "B"), Comp("name", "D"));
    ASSERT_TRUE(s2);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    jsonString.remove(R"({"name":"F"},)");
    auto s3 =
        r.eraseValue(
        "/sections/[?]/sections/[?]", Comp("name", "B"), Comp("name", "F"));
    ASSERT_TRUE(s3);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    QString jsonString2{json.simplified().remove(" ")};
    Processor r2{json.toUtf8()};

    jsonString2.remove(R"({"name":"D","sections":[{"name":"E"}]},)");
    jsonString2.remove(R"({"name":"F"},)");
    auto s4 = r2.eraseAllValues(
        "/sections/[?]/sections/[?]", Comp("name", "B"), Cont("name", QStringList{"D", "F"}));
    ASSERT_EQ(s4, 2);
    ASSERT_EQ(r2.toStdString(), jsonString2.toStdString());
}

TEST(Processor, Modify)
{
    ::rapidjson::Document smallDoc;
    smallDoc.Parse(smallJson.toUtf8());
    ::rapidjson::Value& val = smallDoc.GetObject();
    QString smallJsonString{smallJson.simplified().remove(" ")};

    Processor r{json.toUtf8()};
    QString jsonString{json.simplified().remove(" ")};

    jsonString.replace(R"({"name":"A"})", "1");
    int intVal = 1;
    auto s = r.modifyValue(intVal, "/sections/0");
    ASSERT_TRUE(s);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    jsonString.replace(R"("name":"D")", R"("name":"new name")");
    auto s2 = r.modifyValue(
        "new name", "/sections/[?]/sections/[?]/name", Comp("name", "B"), Comp("name", "D"));
    ASSERT_TRUE(s2);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    jsonString.replace(R"("name":"C")", R"("name":"new name")");
    jsonString.replace(R"("name":"F")", R"("name":"new name")");
    auto s3 = r.modifyAllValues(
        "new name", "/sections/[?]/sections/[?]/name", Comp("name", "B"), Cont("name", QStringList{"C", "F"}));
    ASSERT_EQ(s3, 2);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    Processor r2{json.toUtf8()};
    QString jsonString2{json.simplified().remove(" ")};
    jsonString2.replace(R"({"name":"F"})", smallJsonString);
    jsonString2.replace(R"({"name":"I"})", smallJsonString);
    auto s4 = r2.modifyAllValues(val,
        "/sections/[?]/sections/[?]",
        Cont("name", QStringList{"B", "H"}),
        Cont("name", QStringList{"F", "I"}));
    ASSERT_EQ(s4, 2);
    ASSERT_EQ(r2.toStdString(), jsonString2.toStdString());

    std::vector<QString> newSections{"S1", "S2", "S3"};
    jsonString2.replace(R"("name":"H","sections":[)" + smallJsonString + "]",
        R"("name":"H","sections":["S1","S2","S3"])");
    auto s5 = r2.modifyValue(newSections, "/sections/[?]/sections", Comp("name", "H"));
    ASSERT_EQ(s5, 1);
    ASSERT_EQ(r2.toStdString(), jsonString2.toStdString());

    SimplestElement el{"A"};
    auto s6 = r2.modifyAllValues(el, "/sections/[?]", Comp("name", "A"));
    ASSERT_EQ(s6, 1);
    ASSERT_EQ(r2.toStdString(), jsonString2.toStdString());
}

TEST(Processor, AddToObject)
{
    ::rapidjson::Document smallDoc;
    smallDoc.Parse(smallJson.toUtf8());
    ::rapidjson::Value& val = smallDoc.GetObject();
    QString smallJsonString{smallJson.simplified().remove(" ")};

    Processor r{json.toUtf8()};
    QString jsonString{json.simplified().remove(" ")};

    insertAfterPeviousElement(jsonString, R"("sections":[{"name":"I"}])", R"(,"new field name":"new field")");
    auto s = r.addValueToObject("new field name", "new field", "/sections/2");
    ASSERT_TRUE(s);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    insertAfterPeviousElement(jsonString, R"("name":"A")", R"(,"sections":)" + smallJsonString);
    auto s2 = r.addValueToObject("sections", val, "/sections/[?]", Comp("name", "A"));
    ASSERT_TRUE(s2);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    insertAfterPeviousElement(jsonString, R"("name":"C")", R"(,"sections":"new sections")");
    insertAfterPeviousElement(jsonString, R"("name":"F")", R"(,"sections":"new sections")");
    auto s3 = r.addValueToObjectAll("sections",
        "new sections",
        "/sections/[?]/sections/[?]",
        Comp("name", "B"),
        Cont("name", QStringList{"C", "F"}));
    ASSERT_EQ(s3, 2);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());
}

TEST(Processor, AddToArray)
{
    ::rapidjson::Document smallDoc;
    smallDoc.Parse(smallJson.toUtf8());
    ::rapidjson::Value& val = smallDoc.GetObject();
    QString smallJsonString{smallJson.simplified().remove(" ")};

    Processor r{json.toUtf8()};
    QString jsonString{json.simplified().remove(" ")};

    insertAfterPeviousElement(jsonString,
        R"({"name":"H","sections":[{"name":"I"}]})", "," + smallJsonString);
    auto s = r.addValueToArray(val, "/sections");
    ASSERT_TRUE(s);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    insertAfterPeviousElement(jsonString, R"({"name":"G"})", R"(,"new object")");
    auto s2 = r.addValueToArray("new object", "/sections/[?]/sections", Comp("name", "B"));
    ASSERT_TRUE(s2);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    insertAfterPeviousElement(jsonString, R"("new object")", R"(,"new object2")");
    insertAfterPeviousElement(jsonString, R"({"name":"I"})", R"(,"new object2")");
    auto s3 = r.addValueToArrayAll(
        "new object2",
        "/sections/[?]/sections",
        Cont("name", QStringList{"B", "H"}));
    ASSERT_EQ(s3, 2);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    // One of the found places is object instead of array.
    insertAfterPeviousElement(jsonString, R"({"name":"E"})", R"(,"new object3")");
    auto s4 = r.addValueToArrayAll("new object3",
        "/sections/[?]/sections/[?]/sections",
        Comp("name", "B"), Cont("name", QStringList{"C", "D"}));
    ASSERT_EQ(s4, 1);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    SmallObject obj{.name="new name", .sections={SmallObject{.name="S1"}, SmallObject{.name="S2"}}};
    auto s5 = r.addValueToArray(obj, "/sections");
    insertAfterPeviousElement(jsonString,
        smallJsonString,
        R"(,{"name":"new name","sections":[{"name":"S1","sections":[]},{"name":"S2","sections":[]}]})");
    ASSERT_TRUE(s5);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());

    // Negative case.
    auto s6 = r.addValueToArrayAll("new object4",
        "/sections/[?]/sections/[?]",
        Comp("name", "B"),
        Cont("name", QStringList{"C", "D"}));
    ASSERT_EQ(s6, 0);
    ASSERT_EQ(r.toStdString(), jsonString.toStdString());
}

} // namespace nx::utils::rapidjson::test
