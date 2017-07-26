#include <detection_plugin_interface.h>

#include <string>
#include <cstring>

namespace {

// Alternative impl of DetectionPlugin as a pure software stub.

class Stub: public AbstractDetectionPlugin
{
public:
    Stub();

    virtual ~Stub() override;

    virtual void id(char* idString, int maxIdLength) const override;
    virtual bool hasMetadata() const override;
    virtual void setParams(const Params& params) override;
    
    virtual bool start() override;

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override;
    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override;

private:
    std::string m_id;
    std::string m_modelFile;
    std::string m_deployFile;
    std::string m_cacheFile;
};

Stub::Stub()
{
}

Stub::~Stub()
{
}

void Stub::id(char* idString, int maxIdLength) const
{
    strncpy(idString, m_id.c_str(), maxIdLength);
}

bool Stub::hasMetadata() const
{
    return false;
}

void Stub::setParams(const Params& params)
{
    m_id = params.id;
    m_modelFile = params.modelFile;
    m_deployFile = params.deployFile;
    m_cacheFile = params.cacheFile;
}

bool start()
{
    return true;
}

bool Stub::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    return true;
}

bool Stub::pullRectsForFrame(
    Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs)
{
    *outPtsUs = 0;
    *outRectsCount = 0;
    return false;
}

} // namespace

//-------------------------------------------------------------------------------------------------

extern "C" {

#ifdef _WIN32
    __declspec(dllexport)
#endif
    AbstractDetectionPlugin* createDetectionPluginInstance()
    {
        return new Stub();
    }

} // extern "C"
