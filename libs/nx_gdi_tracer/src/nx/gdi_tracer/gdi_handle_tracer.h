#pragma once

#include <memory>
#include <unordered_map>
#include <filesystem>
#include <windows.h>

namespace nx {
namespace gdi_tracer {

class CallStackProvider;

class GdiHandleTracer
{
public:
    static GdiHandleTracer* getInstance();

    void attachGdiDetours();
    void detachGdiDetours();
    void setReportPath(const std::filesystem::path& path);
    void setGdiTraceLimit(int handleCount);

    void traceBitmap(HBITMAP handle);
    void traceBrush(HBRUSH handle);
    void tracePalette(HPALETTE handle);
    void traceFont(HFONT handle);
    void traceRegion(HRGN handle);
    void removeGdiObject(HGDIOBJ handle);

    void traceMetafile(HDC handle);
    void removeMetafile(HDC handle);

    void traceDc(HDC handle);
    void removeDc(HDC handle);

private:
    GdiHandleTracer();
    bool isReportCreated() const;
    std::string getReport() const;
    void saveReportToFile(const std::filesystem::path& path) const;
    void checkGdiHandlesCount();
    void clear();

private:
    static constexpr unsigned int kTopEntriesStripCount = 3;

    static GdiHandleTracer* m_instance;

    std::unique_ptr<CallStackProvider> m_callStackProvider;
    std::unordered_map<HBITMAP, std::string> m_bitmapHandles;
    std::unordered_map<HBRUSH, std::string> m_brushHandles;
    std::unordered_map<HPALETTE, std::string> m_paletteHandles;
    std::unordered_map<HDC, std::string> m_dcHandles;
    std::unordered_map<HFONT, std::string> m_fontHandles;
    std::unordered_map<HDC, std::string> m_metafileHandles;
    std::unordered_map<HRGN, std::string> m_regionHandles;
    std::filesystem::path m_reportPath;
    int m_gdiTraceLimit = 5000;
    bool m_reportCreated = false;

};

} // namespace gdi_tracer
} // namespace nx
