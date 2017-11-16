#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/std/cpp14.h>

#include <rest/ec2_update_http_handler.h>

#include <nx/core/access/access_types.h>
#include <core/resource_access/user_access_data.h>
#include <api/model/audit/auth_session.h>
#include <nx_ec/data/api_data.h>

#include "mock_stream_socket.h"
#include <network/tcp_listener.h>
#include "network/http_connection_listener.h"
#include <rest/server/rest_connection_processor.h>
#include <common/static_common_module.h>

namespace ec2 {
namespace test {

namespace {

//-------------------------------------------------------------------------------------------------
// Test/mock classes.

class MockQnHttpConnectionListener: public QnHttpConnectionListener
{
public:
    MockQnHttpConnectionListener(QnCommonModule* commonModule):
        QnHttpConnectionListener(commonModule, QHostAddress::Any, /*port*/ 0)
    {
    }

    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<AbstractStreamSocket> clientSocket) override
    {
        return nullptr;
    }
};

struct ApiMockInnerData
{
    ApiMockInnerData(): i(777) {}
    ApiMockInnerData(int _i): i(_i) {}
    bool operator==(const ApiMockInnerData& other) const { return i == other.i; }
    bool operator!=(const ApiMockInnerData& other) const { return !(*this == other); }
    std::string toJsonString() const { return QJson::serialized(*this).toStdString(); }

    int i;
};
#define ApiMockInnerData_Fields (i)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiMockInnerData), (ubjson)(json), _Fields)
typedef std::vector<ApiMockInnerData> ApiMockInnerDataList;

struct ApiMockData: ApiIdData
{
    ApiMockData(): ApiIdData(), i(666) {}

    ApiMockData(
        QnUuid _id, int _i, const ApiMockInnerData& _inner, const ApiMockInnerDataList& _array)
        :
        ApiIdData(_id), i(_i), inner(_inner), array(_array)
    {
    }

    bool operator==(const ApiMockData& other) const
    {
        return id == other.id && i == other.i && inner == other.inner && array == other.array;
    }

    bool operator!=(const ApiMockData& other) const { return !(*this == other); }

    std::string toJsonString() const { return QJson::serialized(*this).toStdString(); }

    int i;
    ApiMockInnerData inner;
    ApiMockInnerDataList array;
};
#define ApiMockData_Fields ApiIdData_Fields (i)(array)(inner)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiMockData), (ubjson)(json), _Fields)
typedef std::vector<ApiMockData> ApiMockDataList;

// Can be initialized either with boost::none or a possibly empty list in figure braces.
struct OptionalIntVector: public boost::optional<std::vector<int>>
{
    typedef boost::optional<std::vector<int>> base_type;
    OptionalIntVector(std::initializer_list<int> list): base_type(std::vector<int>(list)) {}
    OptionalIntVector(boost::none_t none): base_type(none) {}
};

struct ApiMockDataJson
{
    ApiMockDataJson() {}

    ApiMockDataJson(
        boost::optional<QnUuid> _id,
        boost::optional<int> i,
        boost::optional<int> inner,
        const OptionalIntVector& array)
        :
        id(_id),
        isIncomplete(!_id || !i || !inner || !array)
    {
        json = "{";
        if (id)
            json.append(lit("\"id\": \"%1\"").arg(id.get().toString()).toLatin1());
        if (i)
            json.append(lit(", \"i\": %1").arg(i.get()).toLatin1());
        if (inner)
            json.append(lit(", \"inner\": {\"i\": %1}").arg(inner.get()).toLatin1());
        if (array)
        {
            json.append(", \"array\": [");
            bool empty = true;
            for (int v: array.get())
            {
                if (empty)
                    empty = false;
                else
                    json.append(", ");
                json.append(lit("{\"i\": %1}").arg(v).toLatin1());
            }
            json.append("]");
        }
        json.append("}");
    }

    boost::optional<QnUuid> id;
    bool isIncomplete;
    QByteArray json;
};

class MockConnection
{
public:
    typedef std::function<void(ErrorCode, ApiMockDataList)> QueryHandler;
    typedef std::function<void(
        ApiCommand::Value cmdCode, QnUuid input, QueryHandler handler)> QueryCallback;

    typedef std::function<void(ErrorCode)> UpdateHandler;
    typedef std::function<void(
        ApiCommand::Value cmdCode, const ApiMockData& tran, UpdateHandler handler)> UpdateCallback;

    MockConnection(QueryCallback queryCallback, UpdateCallback updateCallback):
        m_queryCallback(queryCallback), m_updateCallback(updateCallback)
    {
        NX_CRITICAL(queryCallback, "[TEST]");
        NX_CRITICAL(updateCallback, "[TEST]");
    }

    template<class InputData, class OutputData, class HandlerType>
    void processQueryAsync(ApiCommand::Value cmdCode, InputData input, HandlerType handler)
    {
        m_queryCallback(cmdCode, input, handler);
    }

    template<class HandlerType>
    void processUpdateAsync(
        ApiCommand::Value cmdCode, const ApiMockData& tranData, HandlerType handler)
    {
        m_updateCallback(cmdCode, tranData, handler);
    }

    MockConnection* queryProcessor() { return this; }
    MockConnection& getAccess(const Qn::UserAccessData& /*accessRights*/) { return *this; }
    MockConnection* auditManager() { return this; }
    void setAuditData(const MockConnection* /*auditManager*/, const QnAuthSession& /*authSession*/)
    {
    }

private:
    const QueryCallback m_queryCallback;
    const UpdateCallback m_updateCallback;
};

typedef UpdateHttpHandler<ApiMockData, MockConnection> TestUpdateHttpHandler;

static const ApiCommand::Value kMockApiCommand = (ApiCommand::Value) 1; //< Any existing command.

//-------------------------------------------------------------------------------------------------
// Test mechanism.

class StructMergingTest
{
public:
    StructMergingTest() {}

    void testValid(
        int sourceCodeLineNumber, const char* sourceCodeLineString,
        const ApiMockDataJson& requestJson,
        const ApiMockData& existingData,
        const ApiMockData& expectedData)
    {
        NX_LOG(lit("[TEST] ====================================================================="),
            cl_logINFO);
        NX_LOG(lit("[TEST] line %1: %2\n").arg(sourceCodeLineNumber).arg(sourceCodeLineString),
            cl_logINFO);
        NX_LOG(lit("[TEST] JSON: %1").arg(requestJson.json.constData()), cl_logDEBUG1);

        m_requestJson = requestJson;
        m_existingData = existingData;
        m_expectedData = expectedData;
        m_wasHandleQueryCalled = false;
        m_wasHandleUpdateCalled = false;

        QByteArray resultBody;
        QByteArray contentType;
        int httpStatusCode = m_updateHttpHandler->executePost(
            /*path*/ "/ec2/" + ApiCommand::toString(kMockApiCommand),
            QnRequestParamList(),
            m_requestJson.json,
            "application/json",
            /*out*/ resultBody,
            /*out*/ contentType,
            &m_restConnectionProcessor);

        ASSERT_EQ("application/json", contentType);
        ASSERT_EQ(nx_http::StatusCode::ok, httpStatusCode);

        bool success = false;
        ApiIdData apiIdData = QJson::deserialized(resultBody, ApiIdData(), &success);
        ASSERT_TRUE(success) << resultBody.toStdString();
        ASSERT_EQ(expectedData.id, apiIdData.id);

        if (m_requestJson.id && m_requestJson.isIncomplete)
            ASSERT_TRUE(m_wasHandleQueryCalled);

        ASSERT_TRUE(m_wasHandleUpdateCalled);
    }

    void testError(
        int sourceCodeLineNumber, const char* sourceCodeLineString,
        const ApiMockDataJson& requestJson,
        const ApiMockData& existingData)
    {
        NX_LOG(lit("[TEST] ====================================================================="),
            cl_logINFO);
        NX_LOG(lit("[TEST] line %1: %2").arg(sourceCodeLineNumber).arg(sourceCodeLineString),
            cl_logINFO);
        NX_LOG(lit("[TEST] JSON: %1").arg(requestJson.json.constData()), cl_logDEBUG1);

        NX_CRITICAL(requestJson.isIncomplete, "[TEST]");

        m_requestJson = requestJson;
        m_existingData = existingData;
        m_wasHandleQueryCalled = false;
        m_wasHandleUpdateCalled = false;

        QByteArray resultBody;
        QByteArray contentType;
        int httpStatusCode = m_updateHttpHandler->executePost(
            /*path*/ "/ec2/" + ApiCommand::toString(kMockApiCommand),
            QnRequestParamList(),
            m_requestJson.json,
            "application/json",
            /*out*/ resultBody,
            /*out*/ contentType,
            &m_restConnectionProcessor);

        ASSERT_EQ("application/json", contentType);
        ASSERT_EQ(nx_http::StatusCode::ok, httpStatusCode);
        ASSERT_FALSE(resultBody.isEmpty());
        ASSERT_FALSE(m_wasHandleQueryCalled);
        ASSERT_FALSE(m_wasHandleUpdateCalled);
    }

private:
    void handleQuery(
        ApiCommand::Value /*command*/, QnUuid input, MockConnection::QueryHandler handler)
    {
        ASSERT_FALSE(m_wasHandleQueryCalled) << "handleQuery() called twice";
        m_wasHandleQueryCalled = true;
        ASSERT_TRUE((bool) m_requestJson.id) << "handleQuery() called but Id omitted from json";

        if (m_requestJson.id.get() != input)
        {
            ADD_FAILURE() << "Request from DB specifies incorrect Id:\n"
                << "Expected: " << m_requestJson.id.get().toStdString() << "\n"
                << "Actual:   " << input.toStdString();
        }

        ApiMockDataList list;
        if (m_existingData.id == input) //< Id is "found" in the mock DB.
            list.push_back(m_existingData);

        handler(ErrorCode::ok, list);
    }

    void handleUpdate(
        ApiCommand::Value command, const ApiMockData& data, MockConnection::UpdateHandler handler)
    {
        ASSERT_FALSE(m_wasHandleUpdateCalled) << "handleUpdate() called twice";
        m_wasHandleUpdateCalled = true;
        NX_CRITICAL(command == kMockApiCommand, "[TEST]");

        NX_LOG(lit("[TEST] Transaction: %1").arg(data.toJsonString().c_str()), cl_logINFO);

        if (m_expectedData != data)
        {
            ADD_FAILURE() << "Expected ApiMockData:\n"
                << m_expectedData.toJsonString() << "\n"
                << "Actual ApiMockData:\n"
                << data.toJsonString();
        }

        handler(ErrorCode::ok);
    }

private:
    QSharedPointer<AbstractStreamSocket> m_socket{new MockStreamSocket()};

    std::shared_ptr<MockConnection> m_connection{new MockConnection(
        [this](ApiCommand::Value command, QnUuid input, MockConnection::QueryHandler handler)
        {
            handleQuery(command, input, handler);
        },
        [this](ApiCommand::Value command, const ApiMockData& data,
            MockConnection::UpdateHandler handler)
        {
            handleUpdate(command, data, handler);
        }
    )};

    QnCommonModule m_commonModule{/*clientMode*/ false, nx::core::access::Mode::direct};
    MockQnHttpConnectionListener listener{&m_commonModule};
    QnRestConnectionProcessor m_restConnectionProcessor{m_socket, /*owner*/ &listener };

    std::unique_ptr<TestUpdateHttpHandler> m_updateHttpHandler{new TestUpdateHttpHandler(
        m_connection)};

    // State of each test().
    ApiMockDataJson m_requestJson;
    ApiMockData m_existingData;
    ApiMockData m_expectedData;
    bool m_wasHandleQueryCalled;
    bool m_wasHandleUpdateCalled;
};

} // namespace

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST(RestApiTest, StructMerging)
{
    StructMergingTest test;

    static const auto n = boost::none;
    static const QnUuid a{"{21f7063b-6106-4339-a218-70b645898c77}"}; //< "Existing" in mock DB.
    static const QnUuid b{"{8c37143f-3fe0-4360-9675-214ede0d6c0d}"}; //< "Non-existing" in mock DB.
    static const QnUuid d{}; //< Default (omitted).
    typedef ApiMockDataJson J;
    typedef ApiMockData D;
    #define T(...) test.testValid(__LINE__, #__VA_ARGS__, __VA_ARGS__)
    #define E(...) test.testError(__LINE__, #__VA_ARGS__, __VA_ARGS__)

    // Request              Existing             Expected
    //  id i  inr array      id i  inr array      id i  inr array         Overrides fields
    T(J(a, n,  n, n     ), D(a, 7, 20, {5   }), D(a, 7, 20, {5   })); //< none
    T(J(a, 3, 21, {1, 2}), D(a, 7, 20, {5, 6}), D(a, 3, 21, {1, 2})); //< all -> new
    T(J(a, 3, 21, {1, 2}), D(a, 3, 21, {1, 2}), D(a, 3, 21, {1, 2})); //< all -> same
    T(J(a, n,  n, {1, 2}), D(a, 7, 20, {5   }), D(a, 7, 20, {1, 2})); //< array -> non-empty
    T(J(a, n,  n, {    }), D(a, 7, 20, {5   }), D(a, 7, 20, {    })); //< array -> empty
    T(J(a, n,  n, {1, 2}), D(a, 7, 20, {    }), D(a, 7, 20, {1, 2})); //< empty array
    T(J(a, n, 21, {1, 2}), D(a, 7, 20, {5   }), D(a, 7, 21, {1, 2})); //< inner, array
    T(J(a, 3,  n, n     ), D(a, 7, 20, {5, 6}), D(a, 3, 20, {5, 6})); //< i
    T(J(a, n, 21, n     ), D(a, 7, 20, {5   }), D(a, 7, 21, {5   })); //< inner
    T(J(b, 3, 21, {1, 2}), D(a, 7, 20, {5, 6}), D(b, 3, 21, {1, 2})); //< none: non-existing id

    // Request              Existing
    //  id i  inr array      id i  inr array         Note
    E(J(n, 3, 21, {1, 2}), D(a, 7, 20, {5, 6})); //< error: omitted id

    #undef E
    #undef T
}

} // namespace test
} // namespace ec2

Q_DECLARE_METATYPE(ec2::test::ApiMockInnerData)
Q_DECLARE_METATYPE(ec2::test::ApiMockData)

