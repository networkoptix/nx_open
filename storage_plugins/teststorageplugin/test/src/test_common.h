#pragma once

#include <test_storage_factory.h>

    /**
    1453550461075 - Sat Jan 23 2016 15:01:01
    */
    const char* const kTestJson = R"JSON(
{
    "sample": "/path/to/sample/file.mkv",
    "cameras": [
        {
            "id": "someCameraId1",
            "hi": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461075"
                },
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461076"
                }
            ],
            "low": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461077"
                } 
            ]
        }
        ,
        {
            "id": "someCameraId2",
            "hi": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461078"
                }
            ],
            "low": [
                {
                    "durationMs": "429626247",
                    "startTimeMs": "1453550461079"
                } 
            ]
        }        
    ]
}
)JSON";

class TestStorageFactoryHelper : public TestStorageFactory
{
public:
    TestStorageFactoryHelper(bool invalidConfig = false) : m_invalidConfig(invalidConfig) {}
    std::string getConfigPath() const { return m_path; }

private:
    virtual bool readConfig(const std::string& path, std::string* outContent)
    {
        if (m_invalidConfig)
            return false;

        m_path = path;
        outContent->assign(kTestJson);

        return true;
    }

    std::string m_path;
    bool m_invalidConfig;
};
