#pragma once

namespace nx::cloud::aws::test {

struct AwsSignatureV4TestData
{
    const char* testName;
    const char* authorizationHeader;
    const char* canonicalRequest;
    const char* request;
    const char* signedRequest;
    const char* stringToSign;
};

static constexpr char kAwsTestUser[] = "AKIDEXAMPLE";
static constexpr char kAwsTestSecretKey[] = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
static constexpr char kAwsTestRegion[] = "us-east-1";
static constexpr char kAwsTestService[] = "service";

/**
 * These tests come from https://docs.aws.amazon.com/general/latest/gr/signature-v4-test-suite.html.
 */
static const AwsSignatureV4TestData kAwsSignatureV4TestSuite[] = {

// post_vanilla_empty_query_value ====================
{
"post_vanilla_empty_query_value",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=28038455d6de14eafc1f9222cf5aa6f1a96197d7deb8263271d420d138af7f11)",

R"(POST
/
Param1=value1
host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST /?Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(POST /?Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=28038455d6de14eafc1f9222cf5aa6f1a96197d7deb8263271d420d138af7f11)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
9d659678c1756bb3113e2ce898845a0a79dbbc57b740555917687f1b3340fbbd)",

},

// post_sts_tokepost_sts_header_after ====================
{
"post_sts_tokepost_sts_header_after",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5da7c1a2acd57cee7505fc6676e4e544621c30862966e37dddb68e92efbe5d6b)",

R"(POST
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
X-Amz-Security-Token:AQoDYXdzEPT//////////wEXAMPLEtc764bNrC9SAPBSM22wDOk4x4HIZ8j4FZTwdQWLWsKWHGBuFqwAeMicRXmxfpSPfIeoIYRqTflfKD8YUuwthAx7mSEI/qkPpKPi/kMcGdQrmGdeehM4IC1NtBmUpp2wUE8phUZampKsburEDy0KPkyQDYwT7WZ0wq5VSXDvp75YU9HFvlRd8Tx6q6fE8YQcHNVXAkiY9q6d+xo0rKwT38xVqr7ZD0u0iPPkUL64lIZbqBAz+scqKmlzm8FDrypNC9Yjc8fPOLn9FX9KSYvKTr4rvx3iSIlTJabIQwj2ICCR/oLxBA==
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5da7c1a2acd57cee7505fc6676e4e544621c30862966e37dddb68e92efbe5d6b)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
553f88c9e4d10fc9e109e2aeb65f030801b70c2f6468faca261d401ae622fc87)",

},

// post_sts_tokepost_sts_header_before ====================
{
"post_sts_tokepost_sts_header_before",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date;x-amz-security-token, Signature=85d96828115b5dc0cfc3bd16ad9e210dd772bbebba041836c64533a82be05ead)",

R"(POST
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z
x-amz-security-token:AQoDYXdzEPT//////////wEXAMPLEtc764bNrC9SAPBSM22wDOk4x4HIZ8j4FZTwdQWLWsKWHGBuFqwAeMicRXmxfpSPfIeoIYRqTflfKD8YUuwthAx7mSEI/qkPpKPi/kMcGdQrmGdeehM4IC1NtBmUpp2wUE8phUZampKsburEDy0KPkyQDYwT7WZ0wq5VSXDvp75YU9HFvlRd8Tx6q6fE8YQcHNVXAkiY9q6d+xo0rKwT38xVqr7ZD0u0iPPkUL64lIZbqBAz+scqKmlzm8FDrypNC9Yjc8fPOLn9FX9KSYvKTr4rvx3iSIlTJabIQwj2ICCR/oLxBA==

host;x-amz-date;x-amz-security-token
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
X-Amz-Security-Token:AQoDYXdzEPT//////////wEXAMPLEtc764bNrC9SAPBSM22wDOk4x4HIZ8j4FZTwdQWLWsKWHGBuFqwAeMicRXmxfpSPfIeoIYRqTflfKD8YUuwthAx7mSEI/qkPpKPi/kMcGdQrmGdeehM4IC1NtBmUpp2wUE8phUZampKsburEDy0KPkyQDYwT7WZ0wq5VSXDvp75YU9HFvlRd8Tx6q6fE8YQcHNVXAkiY9q6d+xo0rKwT38xVqr7ZD0u0iPPkUL64lIZbqBAz+scqKmlzm8FDrypNC9Yjc8fPOLn9FX9KSYvKTr4rvx3iSIlTJabIQwj2ICCR/oLxBA==)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
X-Amz-Security-Token:AQoDYXdzEPT//////////wEXAMPLEtc764bNrC9SAPBSM22wDOk4x4HIZ8j4FZTwdQWLWsKWHGBuFqwAeMicRXmxfpSPfIeoIYRqTflfKD8YUuwthAx7mSEI/qkPpKPi/kMcGdQrmGdeehM4IC1NtBmUpp2wUE8phUZampKsburEDy0KPkyQDYwT7WZ0wq5VSXDvp75YU9HFvlRd8Tx6q6fE8YQcHNVXAkiY9q6d+xo0rKwT38xVqr7ZD0u0iPPkUL64lIZbqBAz+scqKmlzm8FDrypNC9Yjc8fPOLn9FX9KSYvKTr4rvx3iSIlTJabIQwj2ICCR/oLxBA==
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date;x-amz-security-token, Signature=85d96828115b5dc0cfc3bd16ad9e210dd772bbebba041836c64533a82be05ead)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
c237e1b440d4c63c32ca95b5b99481081cb7b13c7e40434868e71567c1a882f6)",

},

// get_header_value_order ====================
{
"get_header_value_order",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=08c7e5a9acfcfeb3ab6b2185e75ce8b1deb5e634ec47601a50643f830c755c01)",

R"(GET
/

host:example.amazonaws.com
my-header1:value4,value1,value3,value2
x-amz-date:20150830T123600Z

host;my-header1;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value4
My-Header1:value1
My-Header1:value3
My-Header1:value2
X-Amz-Date:20150830T123600Z)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value4
My-Header1:value1
My-Header1:value3
My-Header1:value2
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=08c7e5a9acfcfeb3ab6b2185e75ce8b1deb5e634ec47601a50643f830c755c01)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
31ce73cd3f3d9f66977ad3dd957dc47af14df92fcd8509f59b349e9137c58b86)",

},

// post_vanilla ====================
{
"post_vanilla",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5da7c1a2acd57cee7505fc6676e4e544621c30862966e37dddb68e92efbe5d6b)",

R"(POST
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5da7c1a2acd57cee7505fc6676e4e544621c30862966e37dddb68e92efbe5d6b)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
553f88c9e4d10fc9e109e2aeb65f030801b70c2f6468faca261d401ae622fc87)",

},

// get_vanilla_empty_query_key ====================
{
"get_vanilla_empty_query_key",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=a67d582fa61cc504c4bae71f336f98b97f1ea3c7a6bfe1b6e45aec72011b9aeb)",

R"(GET
/
Param1=value1
host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /?Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /?Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=a67d582fa61cc504c4bae71f336f98b97f1ea3c7a6bfe1b6e45aec72011b9aeb)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
1e24db194ed7d0eec2de28d7369675a243488e08526e8c1c73571282f7c517ab)",

},

// get_vanilla_query ====================
{
"get_vanilla_query",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(GET
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
bb579772317eb040ac9ed261061d46c1f17a8133879d6129b6e1c25292927e63)",

},

// get_vanilla_query_order_key ====================
{
"get_vanilla_query_order_key",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=eedbc4e291e521cf13422ffca22be7d2eb8146eecf653089df300a15b2382bd1)",

R"(GET
/
Param1=Value1&Param1=value2
host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /?Param1=value2&Param1=Value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /?Param1=value2&Param1=Value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=eedbc4e291e521cf13422ffca22be7d2eb8146eecf653089df300a15b2382bd1)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
704b4cef673542d84cdff252633f065e8daeba5f168b77116f8b1bcaf3d38f89)",

},

// post_vanilla_query ====================
{
"post_vanilla_query",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=28038455d6de14eafc1f9222cf5aa6f1a96197d7deb8263271d420d138af7f11)",

R"(POST
/
Param1=value1
host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST /?Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(POST /?Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=28038455d6de14eafc1f9222cf5aa6f1a96197d7deb8263271d420d138af7f11)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
9d659678c1756bb3113e2ce898845a0a79dbbc57b740555917687f1b3340fbbd)",

},

// get_header_key_duplicate ====================
{
"get_header_key_duplicate",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=c9d5ea9f3f72853aea855b47ea873832890dbdd183b4468f858259531a5138ea)",

R"(GET
/

host:example.amazonaws.com
my-header1:value2,value2,value1
x-amz-date:20150830T123600Z

host;my-header1;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value2
My-Header1:value2
My-Header1:value1
X-Amz-Date:20150830T123600Z)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value2
My-Header1:value2
My-Header1:value1
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=c9d5ea9f3f72853aea855b47ea873832890dbdd183b4468f858259531a5138ea)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
dc7f04a3abfde8d472b0ab1a418b741b7c67174dad1551b4117b15527fbe966c)",

},

// get_vanilla_query_order_key_case ====================
{
"get_vanilla_query_order_key_case",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=b97d918cfa904a5beff61c982a1b6f458b799221646efd99d3219ec94cdf2500)",

R"(GET
/
Param1=value1&Param2=value2
host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /?Param2=value2&Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /?Param2=value2&Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=b97d918cfa904a5beff61c982a1b6f458b799221646efd99d3219ec94cdf2500)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
816cd5b414d056048ba4f7c5386d6e0533120fb1fcfa93762cf0fc39e2cf19e0)",

},

// normalize_patget_slash_pointless_dot ====================
{
"normalize_patget_slash_pointless_dot",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=ef75d96142cf21edca26f06005da7988e4f8dc83a165a80865db7089db637ec5)",

R"(GET
/example

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /./example HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /./example HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=ef75d96142cf21edca26f06005da7988e4f8dc83a165a80865db7089db637ec5)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
214d50c111a8edc4819da6a636336472c916b5240f51e9a51b5c3305180cf702)",

},

// normalize_patget_relative_relative ====================
{
"normalize_patget_relative_relative",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(GET
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /example1/example2/../.. HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /example1/example2/../.. HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
bb579772317eb040ac9ed261061d46c1f17a8133879d6129b6e1c25292927e63)",

},

// TODO: //example// is parsed with "example" being host
#if 0

// normalize_patget_slashes ====================
{
"normalize_patget_slashes",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=9a624bd73a37c9a373b5312afbebe7a714a789de108f0bdfe846570885f57e84)",

R"(GET
/example/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET //example// HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET //example// HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=9a624bd73a37c9a373b5312afbebe7a714a789de108f0bdfe846570885f57e84)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
cb96b4ac96d501f7c5c15bc6d67b3035061cfced4af6585ad927f7e6c985c015)",

},

#endif

// TODO: "GET /example space/ HTTP/1.1" request line is not parsed.
#if 0

// normalize_patget_space ====================
{
"normalize_patget_space",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=652487583200325589f1fba4c7e578f72c47cb61beeca81406b39ddec1366741)",

R"(GET
/example%20space/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /example space/ HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /example space/ HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=652487583200325589f1fba4c7e578f72c47cb61beeca81406b39ddec1366741)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
63ee75631ed7234ae61b5f736dfc7754cdccfedbff4b5128a915706ee9390d86)",

},

#endif

// normalize_patget_relative ====================
{
"normalize_patget_relative",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(GET
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /example/.. HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /example/.. HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
bb579772317eb040ac9ed261061d46c1f17a8133879d6129b6e1c25292927e63)",

},

// normalize_patget_slash ====================
{
"normalize_patget_slash",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(GET
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET // HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET // HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
bb579772317eb040ac9ed261061d46c1f17a8133879d6129b6e1c25292927e63)",

},

// normalize_patget_slash_dot_slash ====================
{
"normalize_patget_slash_dot_slash",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(GET
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /./ HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /./ HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
bb579772317eb040ac9ed261061d46c1f17a8133879d6129b6e1c25292927e63)",

},

// NOTE: Multi-line header - very exotic case.
#if 0
// get_header_value_multiline ====================
{
"get_header_value_multiline",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=ba17b383a53190154eb5fa66a1b836cc297cc0a3d70a5d00705980573d8ff790)",

R"(GET
/

host:example.amazonaws.com
my-header1:value1,value2,value3
x-amz-date:20150830T123600Z

host;my-header1;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value1
  value2
     value3
X-Amz-Date:20150830T123600Z)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value1
  value2
     value3
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=ba17b383a53190154eb5fa66a1b836cc297cc0a3d70a5d00705980573d8ff790)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
b7b6cbfd8a0430b78891e986784da2630c8a135a8595cec25b26ea94f926ee55)",

},

#endif

// get_vanilla ====================
{
"get_vanilla",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(GET
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
bb579772317eb040ac9ed261061d46c1f17a8133879d6129b6e1c25292927e63)",

},

// post_header_value_case ====================
{
"post_header_value_case",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=cdbc9802e29d2942e5e10b5bccfdd67c5f22c7c4e8ae67b53629efa58b974b7d)",

R"(POST
/

host:example.amazonaws.com
my-header1:VALUE1
x-amz-date:20150830T123600Z

host;my-header1;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
My-Header1:VALUE1
X-Amz-Date:20150830T123600Z)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
My-Header1:VALUE1
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=cdbc9802e29d2942e5e10b5bccfdd67c5f22c7c4e8ae67b53629efa58b974b7d)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
d51ced243e649e3de6ef63afbbdcbca03131a21a7103a1583706a64618606a93)",

},

// post_header_key_case ====================
{
"post_header_key_case",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5da7c1a2acd57cee7505fc6676e4e544621c30862966e37dddb68e92efbe5d6b)",

R"(POST
/

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5da7c1a2acd57cee7505fc6676e4e544621c30862966e37dddb68e92efbe5d6b)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
553f88c9e4d10fc9e109e2aeb65f030801b70c2f6468faca261d401ae622fc87)",

},

// get_vanilla_query_order_value ====================
{
"get_vanilla_query_order_value",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5772eed61e12b33fae39ee5e7012498b51d56abc0abb7c60486157bd471c4694)",

R"(GET
/
Param1=value1&Param1=value2
host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /?Param1=value2&Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /?Param1=value2&Param1=value1 HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5772eed61e12b33fae39ee5e7012498b51d56abc0abb7c60486157bd471c4694)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
c968629d70850097a2d8781c9bf7edcb988b04cac14cca9be4acc3595f884606)",

},

// get_vanilla_query_unreserved ====================
{
"get_vanilla_query_unreserved",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=9c3e54bfcdf0b19771a7f523ee5669cdf59bc7cc0884027167c21bb143a40197)",

R"(GET
/
-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz=-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /?-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz=-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /?-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz=-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=9c3e54bfcdf0b19771a7f523ee5669cdf59bc7cc0884027167c21bb143a40197)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
c30d4703d9f799439be92736156d47ccfb2d879ddf56f5befa6d1d6aab979177)",

},

// post_header_key_sort ====================
{
"post_header_key_sort",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=c5410059b04c1ee005303aed430f6e6645f61f4dc9e1461ec8f8916fdf18852c)",

R"(POST
/

host:example.amazonaws.com
my-header1:value1
x-amz-date:20150830T123600Z

host;my-header1;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value1
X-Amz-Date:20150830T123600Z)",

R"(POST / HTTP/1.1
Host:example.amazonaws.com
My-Header1:value1
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=c5410059b04c1ee005303aed430f6e6645f61f4dc9e1461ec8f8916fdf18852c)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
9368318c2967cf6de74404b30c65a91e8f6253e0a8659d6d5319f1a812f87d65)",

},

// get_header_value_trim ====================
{
"get_header_value_trim",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;my-header2;x-amz-date, Signature=acc3ed3afb60bb290fc8d2dd0098b9911fcaa05412b367055dee359757a9c736)",

R"(GET
/

host:example.amazonaws.com
my-header1:value1
my-header2:"a b c"
x-amz-date:20150830T123600Z

host;my-header1;my-header2;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1: value1
My-Header2: "a   b   c"
X-Amz-Date:20150830T123600Z)",

R"(GET / HTTP/1.1
Host:example.amazonaws.com
My-Header1: value1
My-Header2: "a   b   c"
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;my-header2;x-amz-date, Signature=acc3ed3afb60bb290fc8d2dd0098b9911fcaa05412b367055dee359757a9c736)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
a726db9b0df21c14f559d0a978e563112acb1b9e05476f0a6a1c7d68f28605c7)",

},

// get_unreserved ====================
{
"get_unreserved",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=07ef7494c76fa4850883e2b006601f940f8a34d404d0cfa977f52a65bbf5f24f)",

R"(GET
/-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz

host:example.amazonaws.com
x-amz-date:20150830T123600Z

host;x-amz-date
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)",

R"(GET /-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z)",

R"(GET /-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz HTTP/1.1
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=07ef7494c76fa4850883e2b006601f940f8a34d404d0cfa977f52a65bbf5f24f)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
6a968768eefaa713e2a6b16b589a8ea192661f098f37349f4e2c0082757446f9)",

}

// TODO: #ak These tests are presumable buggy: the canonical request hash is not correct.
// Check with sha256sum command line utility.
#if 0
// post_x_www_form_urlencoded_parameters ====================
{
    "post_x_www_form_urlencoded_parameters",

        R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=content-length;content-type;host;x-amz-date, Signature=1a72ec8f64bd914b0e42e42607c7fbce7fb2c7465f63e3092b3b0d39fa77a6fe)",

        R"(POST
/

content-length:13
content-type:application/x-www-form-urlencoded; charset=utf-8
host:example.amazonaws.com
x-amz-date:20150830T123600Z

content-length;content-type;host;x-amz-date
9095672bbd1f56dfc5b65f3e153adc8731a4a654192329106275f4c7b24d0b6e)",

R"(POST / HTTP/1.1
Content-Type:application/x-www-form-urlencoded; charset=utf-8
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Content-Length:13

Param1=value1)",

R"(POST / HTTP/1.1
Content-Type:application/x-www-form-urlencoded; charset=utf-8
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=content-type;host;x-amz-date, Signature=1a72ec8f64bd914b0e42e42607c7fbce7fb2c7465f63e3092b3b0d39fa77a6fe

Param1=value1)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
2e1cf7ed91881a30569e46552437e4156c823447bf1781b921b5d486c568dd1c)",

},

// post_x_www_form_urlencoded ====================
{
"post_x_www_form_urlencoded",

R"(AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=content-type;host;x-amz-date, Signature=ff11897932ad3f4e8b18135d722051e5ac45fc38421b1da7b9d196a0fe09473a)",

R"(POST
/

content-length:13
content-type:application/x-www-form-urlencoded
host:example.amazonaws.com
x-amz-date:20150830T123600Z

content-length;content-type;host;x-amz-date
9095672bbd1f56dfc5b65f3e153adc8731a4a654192329106275f4c7b24d0b6e)",

R"(POST / HTTP/1.1
Content-Type:application/x-www-form-urlencoded
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Content-Length:13

Param1=value1)",

R"(POST / HTTP/1.1
Content-Type:application/x-www-form-urlencoded
Host:example.amazonaws.com
X-Amz-Date:20150830T123600Z
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=content-type;host;x-amz-date, Signature=ff11897932ad3f4e8b18135d722051e5ac45fc38421b1da7b9d196a0fe09473a

Param1=value1)",

R"(AWS4-HMAC-SHA256
20150830T123600Z
20150830/us-east-1/service/aws4_request
42a5e5bb34198acb3e84da4f085bb7927f2bc277ca766e6d19c73c2154021281)",

},

#endif

};

} // namespace nx::cloud::aws::test
