#define USE_GMOCK

#ifdef _WIN32
#include <WinSock2.h>
#endif

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    #if defined(_WIN32)
        WSADATA wsaData;
        WORD versionRequested = MAKEWORD(2, 0);
        WSAStartup(versionRequested, &wsaData);
    #endif

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
