// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/uuid.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/context.h>

namespace nx::network::ssl::test {

static X509Name kCertificateNameA("Test", "US", "AAA");
static X509Name kCertificateNameB("Test", "US", "BBB");

static const std::string kPublicPem = /*suppress newline*/ 1 + (const char*)
R"text(
-----BEGIN CERTIFICATE-----
MIIFbDCCBFSgAwIBAgIQB7IbDqx+PB2F6luZkUOWNzANBgkqhkiG9w0BAQsFADBG
MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRUwEwYDVQQLEwxTZXJ2ZXIg
Q0EgMUIxDzANBgNVBAMTBkFtYXpvbjAeFw0yMDA3MDgwMDAwMDBaFw0yMTA4MDgx
MjAwMDBaMBwxGjAYBgNVBAMTEWNsb3VkLXRlc3QuaGR3Lm14MIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAmUR/R0lsCIryaWdTocklmJmLoaYEtXvM5r9e
nzfMkJEG/KzVZnfolGl0l9RtMi2ZqfU/IO3CQbuBRnLNsp8KxbxO5a/r63cb0j/o
vybnPjq2OY9CV6mjry5XQy7G7QG65Sp1Zddhr4WVAfg8IkZ/hubFg8XEfxaq3Wbb
VQm+AQTBfbeq+SMl/LwNjb9G4OtdgWV3P8j7Zk5yp1AjtoNwzmm313Fe6TH5pocV
yK5klu5NPEs6qnJ5yufeJmZySSOivgdgSfZEvW9Xkhxo2VE61fE0pTiraA3ND8eL
1aWIMGVeKem0tAg9AC2jOKzjAdL1UPyR7gkYTPSHUAHGefXqrwIDAQABo4ICfjCC
AnowHwYDVR0jBBgwFoAUWaRmBlKge5WSPKOUByeWdFv5PdAwHQYDVR0OBBYEFE7g
mc6shVwUo6qBjjRO934DoXJXMBwGA1UdEQQVMBOCEWNsb3VkLXRlc3QuaGR3Lm14
MA4GA1UdDwEB/wQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIw
OwYDVR0fBDQwMjAwoC6gLIYqaHR0cDovL2NybC5zY2ExYi5hbWF6b250cnVzdC5j
b20vc2NhMWIuY3JsMCAGA1UdIAQZMBcwCwYJYIZIAYb9bAECMAgGBmeBDAECATB1
BggrBgEFBQcBAQRpMGcwLQYIKwYBBQUHMAGGIWh0dHA6Ly9vY3NwLnNjYTFiLmFt
YXpvbnRydXN0LmNvbTA2BggrBgEFBQcwAoYqaHR0cDovL2NydC5zY2ExYi5hbWF6
b250cnVzdC5jb20vc2NhMWIuY3J0MAwGA1UdEwEB/wQCMAAwggEFBgorBgEEAdZ5
AgQCBIH2BIHzAPEAdwD2XJQv0XcwIhRUGAgwlFaO400TGTO/3wwvIAvMTvFk4wAA
AXMr7iWIAAAEAwBIMEYCIQCnvyC6eHKoDTY1LtsORfEZ0r41lWgrEYI/+pGAmdD1
kwIhAK1CNOsNCKkfk4iIdGg8Ddhk12g5LRhnJARSqkk8R6TcAHYAXNxDkv7mq0VE
sV6a1FbmEDf71fpH3KFzlLJe5vbHDsoAAAFzK+4lsQAABAMARzBFAiEA7YGsLbhb
7jBYGgJiBSJP6EPTFP/kRbsWi2sNiJ4Pa8ACICTCPeE6oLeyuzMjbLPuvTqUD0bn
t1zaKaaPHhLqkGhjMA0GCSqGSIb3DQEBCwUAA4IBAQA98WWdgtGqM4SZbgiKCx0K
zUzWqVlouuheKWsAKtadERVmVV8CJtADLUNuDcnJMZnnwdaXF5BoI3lW7QoEhreo
yVFtfZeY975JJ70MqGfxiQQ7ko0549jTbUFHzb2w4duGjXRzduq/JaqJStl3W9dJ
xpkD4oyXunKGCTkuIImwgYZzgIqP3QnKsT7sjtCZM3uQRVvtwKM1jijmAAAFQiOn
owcJWot211n8IBXInrNkEtz0u3D0FHosgiAMrfLn3EEHLiuLnR4DlUzokldSpv7j
JVGWDLtuzhR0eaiXCyDCInhMpdauBB5u7Lo3aHJ3tGXLLL1dwUKF3b0+Kfr+NBws
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIESTCCAzGgAwIBAgITBn+UV4WH6Kx33rJTMlu8mYtWDTANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MTAyMjAwMDAwMFoXDTI1MTAxOTAwMDAwMFowRjEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEVMBMGA1UECxMMU2VydmVyIENB
IDFCMQ8wDQYDVQQDEwZBbWF6b24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK
AoIBAQDCThZn3c68asg3Wuw6MLAd5tES6BIoSMzoKcG5blPVo+sDORrMd4f2AbnZ
cMzPa43j4wNxhplty6aUKk4T1qe9BOwKFjwK6zmxxLVYo7bHViXsPlJ6qOMpFge5
blDP+18x+B26A0piiQOuPkfyDyeR4xQghfj66Yo19V+emU3nazfvpFA+ROz6WoVm
B5x+F2pV8xeKNR7u6azDdU5YVX1TawprmxRC1+WsAYmz6qP+z8ArDITC2FMVy2fw
0IjKOtEXc/VfmtTFch5+AfGYMGMqqvJ6LcXiAhqG5TI+Dr0RtM88k+8XUBCeQ8IG
KuANaL7TiItKZYxK1MMuTJtV9IblAgMBAAGjggE7MIIBNzASBgNVHRMBAf8ECDAG
AQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUWaRmBlKge5WSPKOUByeW
dFv5PdAwHwYDVR0jBBgwFoAUhBjMhTTsvAyUlC4IWZzHshBOCggwewYIKwYBBQUH
AQEEbzBtMC8GCCsGAQUFBzABhiNodHRwOi8vb2NzcC5yb290Y2ExLmFtYXpvbnRy
dXN0LmNvbTA6BggrBgEFBQcwAoYuaHR0cDovL2NydC5yb290Y2ExLmFtYXpvbnRy
dXN0LmNvbS9yb290Y2ExLmNlcjA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3Js
LnJvb3RjYTEuYW1hem9udHJ1c3QuY29tL3Jvb3RjYTEuY3JsMBMGA1UdIAQMMAow
CAYGZ4EMAQIBMA0GCSqGSIb3DQEBCwUAA4IBAQCFkr41u3nPo4FCHOTjY3NTOVI1
59Gt/a6ZiqyJEi+752+a1U5y6iAwYfmXss2lJwJFqMp2PphKg5625kXg8kP2CN5t
6G7bMQcT8C8xDZNtYTd7WPD8UZiRKAJPBXa30/AbwuZe0GaFEQ8ugcYQgSn+IGBI
8/LwhBNTZTUVEWuCUUBVV18YtbAiPq3yXqMB48Oz+ctBWuZSkbvkNodPLamkB2g1
upRyzQ7qDn1X8nn8N8V7YJ6y68AtkHcNSRAnpTitxBKjtKPISLMVCx7i4hncxHZS
yLyKQXhw2W2Xs0qLeC1etA+jTGDK4UfLeC0SF7FSi8o5LL21L8IzApar2pR/
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIEkjCCA3qgAwIBAgITBn+USionzfP6wq4rAfkI7rnExjANBgkqhkiG9w0BAQsF
ADCBmDELMAkGA1UEBhMCVVMxEDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNj
b3R0c2RhbGUxJTAjBgNVBAoTHFN0YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4x
OzA5BgNVBAMTMlN0YXJmaWVsZCBTZXJ2aWNlcyBSb290IENlcnRpZmljYXRlIEF1
dGhvcml0eSAtIEcyMB4XDTE1MDUyNTEyMDAwMFoXDTM3MTIzMTAxMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaOCATEwggEtMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/
BAQDAgGGMB0GA1UdDgQWBBSEGMyFNOy8DJSULghZnMeyEE4KCDAfBgNVHSMEGDAW
gBScXwDfqgHXMCs4iKK4bUqc8hGRgzB4BggrBgEFBQcBAQRsMGowLgYIKwYBBQUH
MAGGImh0dHA6Ly9vY3NwLnJvb3RnMi5hbWF6b250cnVzdC5jb20wOAYIKwYBBQUH
MAKGLGh0dHA6Ly9jcnQucm9vdGcyLmFtYXpvbnRydXN0LmNvbS9yb290ZzIuY2Vy
MD0GA1UdHwQ2MDQwMqAwoC6GLGh0dHA6Ly9jcmwucm9vdGcyLmFtYXpvbnRydXN0
LmNvbS9yb290ZzIuY3JsMBEGA1UdIAQKMAgwBgYEVR0gADANBgkqhkiG9w0BAQsF
AAOCAQEAYjdCXLwQtT6LLOkMm2xF4gcAevnFWAu5CIw+7bMlPLVvUOTNNWqnkzSW
MiGpSESrnO09tKpzbeR/FoCJbM8oAxiDR3mjEH4wW6w7sGDgd9QIpuEdfF7Au/ma
eyKdpwAJfqxGF4PcnCZXmTA5YpaP7dreqsXMGz7KQ2hsVxa81Q4gLv7/wmpdLqBK
bRRYh5TmOTFffHPLkIhqhBGWJ6bt2YFGpn6jcgAKUj6DiAdjd4lpFw85hdKrCEVN
0FE6/V1dN2RMfjCyVSRCnTawXZwXgWHxyvkQAiSr6w10kY17RSlQOYiypok1JR4U
akcjMS9cmvqtmg5iUaQqqcT5NJ0hGA==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV
BAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIw
MAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0
eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJV
UzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UE
ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZp
ZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi
MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/
y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0N
Tm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRo
Ot+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0C
zyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg5J
Q4W2bHO7ZOphQazJ1FTfhy/HIrImzJ9ZVGif/L4qL8RVHHVAYBeFAlU5i38FAgMB
AAGjgfAwge0wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0O
BBYEFJxfAN+qAdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFL9ft9HO3R+G9FtV
rNzXEMIOqYjnME8GCCsGAQUFBwEBBEMwQTAcBggrBgEFBQcwAYYQaHR0cDovL28u
c3MyLnVzLzAhBggrBgEFBQcwAoYVaHR0cDovL3guc3MyLnVzL3guY2VyMCYGA1Ud
HwQfMB0wG6AZoBeGFWh0dHA6Ly9zLnNzMi51cy9yLmNybDARBgNVHSAECjAIMAYG
BFUdIAAwDQYJKoZIhvcNAQELBQADggEBACMd44pXyn3pF3lM8R5V/cxTbj5HD9/G
VfKyBDbtgB9TxF00KGu+x1X8Z+rLP3+QsjPNG1gQggL4+C/1E2DUBc7xgQjB3ad1
l08YuW3e95ORCLp+QCztweq7dp4zBncdDQh/U90bZKuCJ/Fp1U1ervShw3WnWEQt
8jxwmKy6abaVd38PMV4s/KCHOkdp8Hlf9BRUpJVeEXgSYCfOn8J3/yNTd126/+pZ
59vPr5KW7ySaNRB6nJHGDn2Z9j8Z3/VyVOEVqQdZe4O/Ui5GjLIAZHYcSNPYeehu
VsyuLAOQ1xk4meTKCRlb/weWsKh/NEnfVqn3sF/tM+2MR7cwA130A4w=
-----END CERTIFICATE-----
)text";

TEST(SslCertificate, Attributes)
{
    const std::string kPem = /*suppress newline*/ 1 + (const char*)
R"text(
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDFJcOeggs7zfgN
etpLeTGNcSzdJH8crijsCxV9DSgWYsRLEr7Uh7kZ3uVpNIKRk+4HEci0l04H/p0t
arcl9xkaO7LULxMIoO4zjBVRBZz2LnymPWPwwgeRNOZppFWsEzmUzCTIt2Zy49rb
UHYe9tVxyS8BRdwDlJ2gpNg8l7EUlE0O0eIGBXZ3XCfGQZfPR6DdV5ymtDcUs6J+
bLSi59MwiuMvyrfl9hAUL+zbtecoMAAkkbiNOIUXgAGP44vXug96JgaE6nYHu0Ls
p5kcAtWKn5w2CVU01pKSfyiGd5JK+K3zRjyjHgSKFupevwmBW8OrV9HTVa1n81nd
0MsrXKUTAgMBAAECggEALoGuF+JNjBoWffeRL2Szj8426yQE6FsdRDGJLCrCXiWG
cL9HTROHUtcF/z9n3ber2vcTBab7vV8O7PvToJ0dytZyZSOFte9gwcA19xr/4AMv
XHf274CWEGcOQ7WEkEcoujU+KCs7e7bMlhfxBXTs/R6cfAxizmVsmczJReHy4AuB
KNt9XEk7R2xHv0RFaP5gFvgT/2Wab4M8sVr+w7LQL9P9g2b5b1iI7Pb8TiZM79Z4
R4KSvHR2iawU5E6G2DO8TgPDPmHewvEe04dsSn3kQ/z64+rJGtKg2p24AfZBZjja
9Nt8ZrYYVTm2O/2rsAtsPF7xXI9vOQU6aqlAMsD4gQKBgQD94H8oDJn6LoIQVhl6
Ti6Vd4gKAUCUYiXaaj4ed8nt7Jj9C+ZiFnXLNn9q6gIcfGHimwjGr2GozzCnHBpQ
7oqzMDVfjsb0jJWHzh7H6f0+4L5oz+STCL7nZWnmRciIDOZBUv+xFjOcHDI0jYBA
vsiU55gUvl+MzId3NN9E8VkWkQKBgQDGy9H+iB1ExLgxV/r/aZZ5/Rb94+KdpSWG
CEPHqv3F9WMBxTvAiy4ABcNuDgIH50tdWv2MV/2f+QyLqKbNPvvBM+HtVZDGfCST
XgRZaPYLNPQYsPY+iShdhw3t8LM5MgvIAKyG4KmDVQVwgcOoaKRVQDgPSc3cHLSf
CQ589B27YwKBgQDMXq1d+w704/2V0wm9eDRt5ARiMXHgQUZBlicddcbbPgxGIA88
xOHcTamy0mASuFpABhfBgat3Lhr3W1sf7XdAGj3NB+3HLWiuI1KKEiXoORlu3HQf
nPm44t2mHmT4iJHO4latIrc3I0eTIJmvBSYJIIo/oKTgfkFKyAg3wqW50QKBgELk
KlDfNBoDp4bS994hhUSe6LGdkI9DFpE3DreMzb9ihmH+H9D2BBB14ACULhLCvRU4
nMAwi2Lcxl/n69h8LPIhpw/ZDtH6y8PaitQbAU9cDhaQ1QrN1AtEemdp6qSANn6h
22u3BNLwNNhakZ+FNmaJKVPbna1G62/n+DwLWEXJAoGBAMTjQXMg+HNeEiXBnsQL
ud1/qpG1XBf8wA8n0FWjxxOEXmvrgi0fBFu50mg5UlcKdrM9DHE7ccbTdsouK+v7
fmOZdMAWDwnwWFhed5rotjUKirAcB5GrhP0GS2JxzdxP5OuLiwCHsCCx87U+qW59
xScANkp04X31pIWrC3kBHE5m
-----END PRIVATE KEY-----
-----BEGIN CERTIFICATE-----
MIIC9DCCAdygAwIBAgIEPAcm9jANBgkqhkiG9w0BAQsFADApMQswCQYDVQQGEwJV
UzENMAsGA1UEAwwEVGVzdDELMAkGA1UECgwCTlgwHhcNMjEwMTIwMDkyMTIwWhcN
MjExMTEzMDkyMTIwWjApMQswCQYDVQQGEwJVUzENMAsGA1UEAwwEVGVzdDELMAkG
A1UECgwCTlgwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDFJcOeggs7
zfgNetpLeTGNcSzdJH8crijsCxV9DSgWYsRLEr7Uh7kZ3uVpNIKRk+4HEci0l04H
/p0tarcl9xkaO7LULxMIoO4zjBVRBZz2LnymPWPwwgeRNOZppFWsEzmUzCTIt2Zy
49rbUHYe9tVxyS8BRdwDlJ2gpNg8l7EUlE0O0eIGBXZ3XCfGQZfPR6DdV5ymtDcU
s6J+bLSi59MwiuMvyrfl9hAUL+zbtecoMAAkkbiNOIUXgAGP44vXug96JgaE6nYH
u0Lsp5kcAtWKn5w2CVU01pKSfyiGd5JK+K3zRjyjHgSKFupevwmBW8OrV9HTVa1n
81nd0MsrXKUTAgMBAAGjJDAiMAsGA1UdDwQEAwIC/DATBgNVHSUEDDAKBggrBgEF
BQcDATANBgkqhkiG9w0BAQsFAAOCAQEAkqa6ORUxkcYAf6u4OtlPwPZNrbh0h+/+
6X2IFGk7NsAuUs5bPno8vYyK1lQ7IZFq2ajyknrgVStlTtr5qgtBzkhexkdGn3QC
Uy5ypeixi9YKsAFVxYBz9She8dlTEwHZt8hWPS5AReTfktUL/iqqL1xwAAlXU+BG
EZgUIZ8Yorw9wUwijyTJ4AWsRTqKRPebRykz92kGXiSiRxS2ZXjWO62XZRvqjD2z
uH/yIFDQRd3J97SIvSOD2ruUsgPmlMpA4K2sIKkJgBbVWoj8rvvwL+TMyXVRsK+O
78Z6S7SZkhEEWuzeVp42fk6fJLc5U+ZWTam/Wz89sqw+8+xdSGGONg==
-----END CERTIFICATE-----
)text";
    const std::vector<unsigned char> kSignature = {
         0x92, 0xa6, 0xba, 0x39, 0x15, 0x31, 0x91, 0xc6, 0x00, 0x7f, 0xab, 0xb8, 0x3a, 0xd9, 0x4f,
         0xc0, 0xf6, 0x4d, 0xad, 0xb8, 0x74, 0x87, 0xef, 0xfe, 0xe9, 0x7d, 0x88, 0x14, 0x69, 0x3b,
         0x36, 0xc0, 0x2e, 0x52, 0xce, 0x5b, 0x3e, 0x7a, 0x3c, 0xbd, 0x8c, 0x8a, 0xd6, 0x54, 0x3b,
         0x21, 0x91, 0x6a, 0xd9, 0xa8, 0xf2, 0x92, 0x7a, 0xe0, 0x55, 0x2b, 0x65, 0x4e, 0xda, 0xf9,
         0xaa, 0x0b, 0x41, 0xce, 0x48, 0x5e, 0xc6, 0x47, 0x46, 0x9f, 0x74, 0x02, 0x53, 0x2e, 0x72,
         0xa5, 0xe8, 0xb1, 0x8b, 0xd6, 0x0a, 0xb0, 0x01, 0x55, 0xc5, 0x80, 0x73, 0xf5, 0x28, 0x5e,
         0xf1, 0xd9, 0x53, 0x13, 0x01, 0xd9, 0xb7, 0xc8, 0x56, 0x3d, 0x2e, 0x40, 0x45, 0xe4, 0xdf,
         0x92, 0xd5, 0x0b, 0xfe, 0x2a, 0xaa, 0x2f, 0x5c, 0x70, 0x00, 0x09, 0x57, 0x53, 0xe0, 0x46,
         0x11, 0x98, 0x14, 0x21, 0x9f, 0x18, 0xa2, 0xbc, 0x3d, 0xc1, 0x4c, 0x22, 0x8f, 0x24, 0xc9,
         0xe0, 0x05, 0xac, 0x45, 0x3a, 0x8a, 0x44, 0xf7, 0x9b, 0x47, 0x29, 0x33, 0xf7, 0x69, 0x06,
         0x5e, 0x24, 0xa2, 0x47, 0x14, 0xb6, 0x65, 0x78, 0xd6, 0x3b, 0xad, 0x97, 0x65, 0x1b, 0xea,
         0x8c, 0x3d, 0xb3, 0xb8, 0x7f, 0xf2, 0x20, 0x50, 0xd0, 0x45, 0xdd, 0xc9, 0xf7, 0xb4, 0x88,
         0xbd, 0x23, 0x83, 0xda, 0xbb, 0x94, 0xb2, 0x03, 0xe6, 0x94, 0xca, 0x40, 0xe0, 0xad, 0xac,
         0x20, 0xa9, 0x09, 0x80, 0x16, 0xd5, 0x5a, 0x88, 0xfc, 0xae, 0xfb, 0xf0, 0x2f, 0xe4, 0xcc,
         0xc9, 0x75, 0x51, 0xb0, 0xaf, 0x8e, 0xef, 0xc6, 0x7a, 0x4b, 0xb4, 0x99, 0x92, 0x11, 0x04,
         0x5a, 0xec, 0xde, 0x56, 0x9e, 0x36, 0x7e, 0x4e, 0x9f, 0x24, 0xb7, 0x39, 0x53, 0xe6, 0x56,
         0x4d, 0xa9, 0xbf, 0x5b, 0x3f, 0x3d, 0xb2, 0xac, 0x3e, 0xf3, 0xec, 0x5d, 0x48, 0x61, 0x8e,
         0x36
    };
    const std::vector<unsigned char> kModulus = {
        0xc5, 0x25, 0xc3, 0x9e, 0x82, 0x0b, 0x3b, 0xcd, 0xf8, 0x0d, 0x7a, 0xda, 0x4b, 0x79, 0x31,
        0x8d, 0x71, 0x2c, 0xdd, 0x24, 0x7f, 0x1c, 0xae, 0x28, 0xec, 0x0b, 0x15, 0x7d, 0x0d, 0x28,
        0x16, 0x62, 0xc4, 0x4b, 0x12, 0xbe, 0xd4, 0x87, 0xb9, 0x19, 0xde, 0xe5, 0x69, 0x34, 0x82,
        0x91, 0x93, 0xee, 0x07, 0x11, 0xc8, 0xb4, 0x97, 0x4e, 0x07, 0xfe, 0x9d, 0x2d, 0x6a, 0xb7,
        0x25, 0xf7, 0x19, 0x1a, 0x3b, 0xb2, 0xd4, 0x2f, 0x13, 0x08, 0xa0, 0xee, 0x33, 0x8c, 0x15,
        0x51, 0x05, 0x9c, 0xf6, 0x2e, 0x7c, 0xa6, 0x3d, 0x63, 0xf0, 0xc2, 0x07, 0x91, 0x34, 0xe6,
        0x69, 0xa4, 0x55, 0xac, 0x13, 0x39, 0x94, 0xcc, 0x24, 0xc8, 0xb7, 0x66, 0x72, 0xe3, 0xda,
        0xdb, 0x50, 0x76, 0x1e, 0xf6, 0xd5, 0x71, 0xc9, 0x2f, 0x01, 0x45, 0xdc, 0x03, 0x94, 0x9d,
        0xa0, 0xa4, 0xd8, 0x3c, 0x97, 0xb1, 0x14, 0x94, 0x4d, 0x0e, 0xd1, 0xe2, 0x06, 0x05, 0x76,
        0x77, 0x5c, 0x27, 0xc6, 0x41, 0x97, 0xcf, 0x47, 0xa0, 0xdd, 0x57, 0x9c, 0xa6, 0xb4, 0x37,
        0x14, 0xb3, 0xa2, 0x7e, 0x6c, 0xb4, 0xa2, 0xe7, 0xd3, 0x30, 0x8a, 0xe3, 0x2f, 0xca, 0xb7,
        0xe5, 0xf6, 0x10, 0x14, 0x2f, 0xec, 0xdb, 0xb5, 0xe7, 0x28, 0x30, 0x00, 0x24, 0x91, 0xb8,
        0x8d, 0x38, 0x85, 0x17, 0x80, 0x01, 0x8f, 0xe3, 0x8b, 0xd7, 0xba, 0x0f, 0x7a, 0x26, 0x06,
        0x84, 0xea, 0x76, 0x07, 0xbb, 0x42, 0xec, 0xa7, 0x99, 0x1c, 0x02, 0xd5, 0x8a, 0x9f, 0x9c,
        0x36, 0x09, 0x55, 0x34, 0xd6, 0x92, 0x92, 0x7f, 0x28, 0x86, 0x77, 0x92, 0x4a, 0xf8, 0xad,
        0xf3, 0x46, 0x3c, 0xa3, 0x1e, 0x04, 0x8a, 0x16, 0xea, 0x5e, 0xbf, 0x09, 0x81, 0x5b, 0xc3,
        0xab, 0x57, 0xd1, 0xd3, 0x55, 0xad, 0x67, 0xf3, 0x59, 0xdd, 0xd0, 0xcb, 0x2b, 0x5c, 0xa5,
        0x13
    };
    const std::vector<unsigned char> kExponent = {1, 0, 1};

    const std::vector<unsigned char> kSha1 = {
        0x57, 0x44, 0xf7, 0x86, 0x79, 0x01, 0x53, 0xbe, 0x1c, 0x90, 0x31, 0xce, 0x1e, 0x87, 0x60,
        0x03, 0xaa, 0xee, 0x7d, 0x76};
    const std::vector<unsigned char> kSha256 = {
        0x72, 0x77, 0xd0, 0x6e, 0xd2, 0xda, 0xc5, 0x4a, 0xc7, 0x94, 0xf0, 0x38, 0x19, 0x97, 0x2e,
        0xda, 0x35, 0xf4, 0x01, 0x9f, 0xc3, 0x49, 0x21, 0x67, 0xb7, 0x5e, 0x3e, 0x39, 0xc0, 0xb4,
        0x0b, 0x70
    };

    Pem pem;
    ASSERT_TRUE(pem.parse(kPem));
    EXPECT_EQ(pem.toString(), "C=US, CN=Test, O=NX");

    const auto& x509 = pem.certificate();
    EXPECT_EQ(x509.toString(), "C=US, CN=Test, O=NX");
    EXPECT_TRUE(x509.isSignedBy(X509Name("Test", "US", "NX")));
    EXPECT_FALSE(x509.isSignedBy(kCertificateNameA));
    EXPECT_FALSE(x509.isSignedBy(kCertificateNameB));

    const auto duration = x509.duration();
    ASSERT_TRUE(duration);
    EXPECT_EQ(*duration, std::chrono::hours(24) * 297);

    const auto& certificates = x509.certificates();
    ASSERT_EQ(certificates.size(), 1);

    const auto& certificate = certificates.front();
    EXPECT_EQ(certificate.version(), 2);
    EXPECT_EQ(certificate.serialNumber(), 1007101686);
    EXPECT_EQ(certificate.signatureAlgorithm(), std::string(LN_sha256WithRSAEncryption));
    EXPECT_EQ(certificate.signature(), kSignature);
    EXPECT_EQ(certificate.issuer(), X509Name("Test", "US", "NX"));
    EXPECT_EQ(certificate.subject(), X509Name("Test", "US", "NX"));
    const auto& extensions = certificate.extensions();
    ASSERT_EQ(extensions.size(), 2);
    EXPECT_EQ(extensions[0].name(), std::string(LN_key_usage));
    EXPECT_EQ(extensions[0].value,
        "Digital Signature, Non Repudiation, Key Encipherment, Data Encipherment, Key Agreement, "
        "Certificate Sign");
    EXPECT_FALSE(extensions[0].isCritical);
    EXPECT_EQ(extensions[1].name(), std::string(LN_ext_key_usage));
    EXPECT_EQ(extensions[1].value, "TLS Web Server Authentication");
    EXPECT_FALSE(extensions[1].isCritical);

    const auto& publicKeyInfo = certificate.publicKeyInformation();
    EXPECT_EQ(publicKeyInfo.algorithm(), std::string(LN_rsaEncryption));
    EXPECT_EQ(publicKeyInfo.modulus, kModulus);
    const auto& rsa = publicKeyInfo.rsa;
    ASSERT_TRUE(rsa);
    EXPECT_EQ(rsa->exponent, kExponent);
    EXPECT_EQ(rsa->bits, 2048);

    EXPECT_EQ(certificate.sha1(), kSha1);
    EXPECT_EQ(certificate.sha256(), kSha256);
}

TEST(SslCertificate, Expiration)
{
    const auto checkExpiration =
        [](const char* label, std::chrono::seconds notBeforeAdj, std::chrono::seconds notAfterAdj)
        {
            Pem pem;
            ASSERT_TRUE(pem.parse(makeCertificateAndKey(
                kCertificateNameA, "localhost", std::nullopt, notBeforeAdj, notAfterAdj))) << label;
            EXPECT_FALSE(pem.certificate().isValid()) << label;
        };

    checkExpiration("expired", -2 * kCertMaxDuration, -1 * kCertMaxDuration);
    checkExpiration("from the future", 1 * kCertMaxDuration, 2 * kCertMaxDuration);
    checkExpiration("invalid duration", std::chrono::seconds::zero(), 2 * kCertMaxDuration);
}

TEST(SslCertificate, CertificatesSignedBySamePrivateKeyHaveSamePublicKey)
{
    const auto pemString1 = makeCertificateAndKey({"Test1", "US", "NX"});
    Pem pem1;
    ASSERT_TRUE(pem1.parse(pemString1));
    const auto pemString2 = makeCertificate(
        pem1.privateKey(),
        {"Test2", "US", "NX"},
        "localhost",
        std::nullopt,
        std::chrono::seconds(100));
    Pem pem2;
    ASSERT_TRUE(pem2.parse(pemString2));

    EXPECT_EQ(pem1.toString(), "C=US, CN=Test1, O=NX");
    EXPECT_EQ(pem2.toString(), "C=US, CN=Test2, O=NX");
    EXPECT_EQ(
        Certificate(pem1.certificate().certificates()[0]).publicKey(),
        Certificate(pem2.certificate().certificates()[0]).publicKey());

    EXPECT_NE(
        Certificate(pem1.certificate().certificates()[0]).serialNumber(),
        Certificate(pem2.certificate().certificates()[0]).serialNumber());
    EXPECT_NE(
        Certificate(pem1.certificate().certificates()[0]).notBefore(),
        Certificate(pem2.certificate().certificates()[0]).notBefore());
}

TEST(SslCertificate, VerifyBySystemCertificatesGood)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf((void*) kPublicPem.data(), kPublicPem.size()), &BIO_free);
    auto chain = utils::wrapUnique(
        sk_X509_new(nullptr),
        [](STACK_OF(X509) * chain) { sk_X509_pop_free(chain, &X509_free); });
    for (int i = 0;; ++i)
    {
        auto x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
        const int x509Size = i2d_X509(x509.get(), 0);
        if (!x509 || x509Size <= 0)
            break;
        sk_X509_insert(chain.get(), x509.release(), i);
    }
    ASSERT_EQ(sk_X509_num(chain.get()), 4);

    if (X509Certificate x509(sk_X509_value(chain.get(), 0)); !x509.isValid())
    {
        std::cerr << "ERROR: The test is aborted because the certificate became invalid. "
            "Update the certificate in the test.\n";
        return;
    }

    std::string errorMessage;
    ASSERT_TRUE(verifyBySystemCertificates(chain.get(), "cloud-test.hdw.mx", &errorMessage))
        << errorMessage;

    ASSERT_FALSE(verifyBySystemCertificates(chain.get(), "nxvms.com", &errorMessage));

    static const char* kErrorMessageStart =
        "Verify certificate for host `nxvms.com` errors: "
        "{ The host name did not match any of the valid hosts for this certificate }. Chain: ";
    ASSERT_TRUE(nx::utils::startsWith(errorMessage, kErrorMessageStart)) << errorMessage;
}

TEST(SslCertificate, VerifyBySystemCertificatesBad)
{
    Pem pem;
    ASSERT_TRUE(pem.parse(makeCertificateAndKey(kCertificateNameA)));
    const auto pemString = pem.certificate().pemString();
    auto bio = utils::wrapUnique(BIO_new_mem_buf(pemString.data(), pemString.size()), &BIO_free);
    auto x509 = utils::wrapUnique(PEM_read_bio_X509_AUX(bio.get(), 0, 0, 0), &X509_free);
    auto chain = utils::wrapUnique(
        sk_X509_new(nullptr),
        [](STACK_OF(X509) * chain) { sk_X509_pop_free(chain, &X509_free); });
    sk_X509_insert(chain.get(), x509.release(), 0);

    std::string errorMessage;
    ASSERT_FALSE(
        verifyBySystemCertificates(chain.get(), /*hostName*/ std::string(), &errorMessage));

    static const char* kErrorMessageStart =
        "Verify certificate for host `` errors: "
        "{ The certificate is self-signed, and untrusted }. Chain: ";
    ASSERT_TRUE(nx::utils::startsWith(errorMessage, kErrorMessageStart)) << errorMessage;
}

TEST(SslCertificate, Hosts)
{
    X509Certificate certificate;
    ASSERT_TRUE(certificate.parsePem(kPublicPem));
    ASSERT_EQ(std::set<std::string>{"cloud-test.hdw.mx"}, certificate.hosts());

    const auto kHostName = nx::Uuid::createUuid().toSimpleStdString();
    const auto kPem = makeCertificateAndKey(kCertificateNameA, kHostName);
    Pem pem;
    ASSERT_TRUE(pem.parse(kPem));
    ASSERT_EQ(std::set<std::string>{kHostName}, pem.certificate().hosts());

    ASSERT_TRUE(nx::network::ssl::Context::instance()->configureVirtualHost(kHostName, kPem));

    const std::set<std::string> kMultipleHostNames = {"cloud-test.hdw.mx", "*.cloud-test.hdw.mx"};
    const std::string kMultipleHostsString = "cloud-test.hdw.mx, *.cloud-test.hdw.mx";

    ASSERT_TRUE(pem.parse(makeCertificateAndKey(kCertificateNameA, kMultipleHostsString)));
    ASSERT_EQ(kMultipleHostNames, pem.certificate().hosts());

    const std::string kMultipleHostsStringWithTwoPrefixes =
        "DNS:cloud-test.hdw.mx, DNS:*.cloud-test.hdw.mx";
    ASSERT_TRUE(pem.parse(makeCertificateAndKey(
        kCertificateNameA,
        kMultipleHostsStringWithTwoPrefixes)));
    ASSERT_EQ(kMultipleHostNames, pem.certificate().hosts());

    const std::string kMultipleHostsStringWithFirstPrefix =
        "DNS:cloud-test.hdw.mx, *.cloud-test.hdw.mx";
    ASSERT_TRUE(pem.parse(makeCertificateAndKey(
        kCertificateNameA,
        kMultipleHostsStringWithFirstPrefix)));
    ASSERT_EQ(kMultipleHostNames, pem.certificate().hosts());

    const std::string kMultipleHostsStringWithSecondPrefix =
        "cloud-test.hdw.mx, DNS:*.cloud-test.hdw.mx";
    ASSERT_TRUE(pem.parse(makeCertificateAndKey(
        kCertificateNameA,
        kMultipleHostsStringWithSecondPrefix)));
    ASSERT_EQ(kMultipleHostNames, pem.certificate().hosts());
}

TEST(SslCertificate, SubjectAltNames)
{
    Pem pem;
    ASSERT_TRUE(pem.parse(makeCertificateAndKey(kCertificateNameA)));
    ASSERT_EQ(
        "DNS:localhost, IP Address:127.0.0.1, IP Address:0:0:0:0:0:0:0:1",
        pem.certificate().subjectAltNames());
}

TEST(SslCertificate, X509Name)
{
    static const X509Name kIssuer("Test", "US", "NX, Inc");
    Pem pem;
    ASSERT_TRUE(pem.parse(makeCertificateAndKey(kIssuer)));
    ASSERT_TRUE(pem.certificate().isSignedBy(kIssuer));
}

TEST(SslCertificate, PrintedText)
{
    X509Certificate chain;
    ASSERT_TRUE(chain.parsePem(kPublicPem));

    for (const auto& certificate: chain.certificates())
    {
        const auto printed = certificate.printedText();

        const static std::string kCertificate = "Certificate:\n";
        ASSERT_TRUE(printed.starts_with(kCertificate));
    }
}

} // namespace nx::network::ssl::test
