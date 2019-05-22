#include "gdi_detours.h"

#include "call_stack_provider.h"
#include <windows.h>
#include <detours.h>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace nx {
namespace gdi_detours {

GdiHandleTracer* GdiHandleTracer::m_instance = nullptr;

// Pointers to bitmap API functions.

HBITMAP (__stdcall* API_CreateBitmap)(int a0,
    int a1,
    UINT a2,
    UINT a3,
    CONST void* a4)
    = CreateBitmap;

HBITMAP (__stdcall* API_CreateBitmapIndirect)(CONST BITMAP* a0)
    = CreateBitmapIndirect;

HBITMAP (__stdcall* API_CreateCompatibleBitmap)(HDC a0,
    int a1,
    int a2)
    = CreateCompatibleBitmap;

HBITMAP (__stdcall* API_CreateDIBSection)(HDC a0,
    CONST BITMAPINFO* a1,
    UINT a2,
    void** a3,
    HANDLE a4,
    DWORD a5)
    = CreateDIBSection;

HBITMAP (__stdcall* API_CreateDIBitmap)(HDC a0,
    CONST BITMAPINFOHEADER* a1,
    DWORD a2,
    CONST void* a3,
    CONST BITMAPINFO* a4,
    UINT a5)
    = CreateDIBitmap;

HBITMAP (__stdcall* API_CreateDiscardableBitmap)(HDC a0,
    int a1,
    int a2)
    = CreateDiscardableBitmap;

// Pointers to brush API functions.

HBRUSH (__stdcall* API_CreateBrushIndirect)(CONST LOGBRUSH* a0)
    = CreateBrushIndirect;

HBRUSH (__stdcall* API_CreateDIBPatternBrush)(HGLOBAL a0,
    UINT a1)
    = CreateDIBPatternBrush;

HBRUSH (__stdcall* API_CreateDIBPatternBrushPt)(CONST void* a0,
    UINT a1)
    = CreateDIBPatternBrushPt;

HBRUSH (__stdcall* API_CreateHatchBrush)(int a0,
    COLORREF a1)
    = CreateHatchBrush;

HBRUSH (__stdcall* API_CreatePatternBrush)(HBITMAP a0)
    = CreatePatternBrush;

HBRUSH (__stdcall* API_CreateSolidBrush)(COLORREF a0)
    = CreateSolidBrush;

// Pointers to palette API functions.

HPALETTE(__stdcall* API_CreateHalftonePalette)(HDC a0)
    = CreateHalftonePalette;

HPALETTE(__stdcall* API_CreatePalette)(CONST LOGPALETTE* a0)
    = CreatePalette;

// Pointers to device context API functions.

HDC (__stdcall* API_CreateCompatibleDC)(HDC a0)
    = CreateCompatibleDC;

HDC (__stdcall* API_CreateDCA)(LPCSTR a0,
    LPCSTR a1,
    LPCSTR a2,
    CONST DEVMODEA * a3)
    = CreateDCA;

HDC (__stdcall* API_CreateDCW)(LPCWSTR a0,
    LPCWSTR a1,
    LPCWSTR a2,
    CONST DEVMODEW * a3)
    = CreateDCW;

HDC (__stdcall* API_CreateICA)(LPCSTR a0,
    LPCSTR a1,
    LPCSTR a2,
    CONST DEVMODEA* a3)
    = CreateICA;

HDC (__stdcall* API_CreateICW)(LPCWSTR a0,
    LPCWSTR a1,
    LPCWSTR a2,
    CONST DEVMODEW* a3)
    = CreateICW;

// Pointers to font API functions.

HFONT (__stdcall* API_CreateFontA)(int a0,
    int a1,
    int a2,
    int a3,
    int a4,
    DWORD a5,
    DWORD a6,
    DWORD a7,
    DWORD a8,
    DWORD a9,
    DWORD a10,
    DWORD a11,
    DWORD a12,
    LPCSTR a13)
    = CreateFontA;

HFONT (__stdcall* API_CreateFontIndirectA)(CONST LOGFONTA* a0)
    = CreateFontIndirectA;

HFONT (__stdcall* API_CreateFontIndirectExA)(CONST ENUMLOGFONTEXDVA* a0)
    = CreateFontIndirectExA;

HFONT (__stdcall* API_CreateFontIndirectExW)(CONST ENUMLOGFONTEXDVW* a0)
    = CreateFontIndirectExW;

HFONT (__stdcall* API_CreateFontIndirectW)(CONST LOGFONTW* a0)
    = CreateFontIndirectW;

HFONT (__stdcall* API_CreateFontW)(int a0,
    int a1,
    int a2,
    int a3,
    int a4,
    DWORD a5,
    DWORD a6,
    DWORD a7,
    DWORD a8,
    DWORD a9,
    DWORD a10,
    DWORD a11,
    DWORD a12,
    LPCWSTR a13)
    = CreateFontW;

// Pointers to metafile API functions.

HDC (__stdcall* API_CreateEnhMetaFileA)(HDC a0,
    LPCSTR a1,
    CONST RECT* a2,
    LPCSTR a3)
    = CreateEnhMetaFileA;

HDC (__stdcall* API_CreateEnhMetaFileW)(HDC a0,
    LPCWSTR a1,
    CONST RECT* a2,
    LPCWSTR a3)
    = CreateEnhMetaFileW;

HDC (__stdcall* API_CreateMetaFileA)(LPCSTR a0)
    = CreateMetaFileA;

HDC (__stdcall* API_CreateMetaFileW)(LPCWSTR a0)
    = CreateMetaFileW;

// Pointers to region API functions.

HRGN (__stdcall* API_CreateEllipticRgn)(int a0,
    int a1,
    int a2,
    int a3)
    = CreateEllipticRgn;

HRGN (__stdcall* API_CreateEllipticRgnIndirect)(CONST RECT* a0)
    = CreateEllipticRgnIndirect;

HRGN(__stdcall* API_CreatePolygonRgn)(CONST POINT* a0,
    int a1,
    int a2)
    = CreatePolygonRgn;

HRGN (__stdcall* API_CreatePolyPolygonRgn)(CONST POINT* a0,
    CONST INT* a1,
    int a2,
    int a3)
    = CreatePolyPolygonRgn;


HRGN (__stdcall* API_CreateRectRgn)(int a0,
    int a1,
    int a2,
    int a3)
    = CreateRectRgn;

HRGN (__stdcall* API_CreateRectRgnIndirect)(CONST RECT* a0)
    = CreateRectRgnIndirect;

HRGN (__stdcall* API_CreateRoundRectRgn)(int a0,
    int a1,
    int a2,
    int a3,
    int a4,
    int a5)
    = CreateRoundRectRgn;

HRGN (__stdcall* API_ExtCreateRegion)(CONST XFORM* a0,
    DWORD a1,
    CONST RGNDATA* a2)
    = ExtCreateRegion;

HRGN (__stdcall* API_PathToRegion)(HDC a0)
    = PathToRegion;

// Pointers to GDI handle release API functions.

BOOL (__stdcall* API_DeleteDC)(HDC a0)
    = DeleteDC;

BOOL (__stdcall* API_DeleteObject)(HGDIOBJ a0)
    = DeleteObject;

BOOL (__stdcall* API_DeleteMetaFile)(HMETAFILE a0)
    = DeleteMetaFile;

BOOL (__stdcall* API_DeleteEnhMetaFile)(HENHMETAFILE a0)
    = DeleteEnhMetaFile;

// Bitmap API detour functions.

HBITMAP __stdcall Detour_CreateBitmap(int a0,
    int a1,
    UINT a2,
    UINT a3,
    void* a4)
{
    auto rv = API_CreateBitmap(a0, a1, a2, a3, a4);
    GdiHandleTracer::getInstance()->traceBitmap(rv);
    return rv;
}

HBITMAP __stdcall Detour_CreateBitmapIndirect(BITMAP* a0)
{
    auto rv = API_CreateBitmapIndirect(a0);
    GdiHandleTracer::getInstance()->traceBitmap(rv);
    return rv;
}

HBITMAP __stdcall Detour_CreateCompatibleBitmap(HDC a0,
    int a1,
    int a2)
{
    auto rv = API_CreateCompatibleBitmap(a0, a1, a2);
    GdiHandleTracer::getInstance()->traceBitmap(rv);
    return rv;
}

HBITMAP __stdcall Detour_CreateDIBSection(HDC a0,
    BITMAPINFO* a1,
    UINT a2,
    void** a3,
    HANDLE a4,
    DWORD a5)
{
    auto rv = API_CreateDIBSection(a0, a1, a2, a3, a4, a5);
    GdiHandleTracer::getInstance()->traceBitmap(rv);
    return rv;
}

HBITMAP __stdcall Detour_CreateDIBitmap(HDC a0,
    BITMAPINFOHEADER* a1,
    DWORD a2,
    void* a3,
    BITMAPINFO* a4,
    UINT a5)
{
    auto rv = API_CreateDIBitmap(a0, a1, a2, a3, a4, a5);
    GdiHandleTracer::getInstance()->traceBitmap(rv);
    return rv;
}


HBITMAP __stdcall Detour_CreateDiscardableBitmap(HDC a0,
    int a1,
    int a2)
{
    auto rv = API_CreateDiscardableBitmap(a0, a1, a2);
    GdiHandleTracer::getInstance()->traceBitmap(rv);
    return rv;
}

// Brush API detour functions.

HBRUSH __stdcall Detour_CreateBrushIndirect(LOGBRUSH* a0)
{
    auto rv = API_CreateBrushIndirect(a0);
    GdiHandleTracer::getInstance()->traceBrush(rv);
    return rv;
}

HBRUSH __stdcall Detour_CreateDIBPatternBrush(HGLOBAL a0,
    UINT a1)
{
    auto rv = API_CreateDIBPatternBrush(a0, a1);
    GdiHandleTracer::getInstance()->traceBrush(rv);
    return rv;
}

HBRUSH __stdcall Detour_CreateDIBPatternBrushPt(void* a0,
    UINT a1)
{
    auto rv = API_CreateDIBPatternBrushPt(a0, a1);
    GdiHandleTracer::getInstance()->traceBrush(rv);
    return rv;
}

HBRUSH __stdcall Detour_CreateHatchBrush(int a0,
    COLORREF a1)
{
    auto rv = API_CreateHatchBrush(a0, a1);
    GdiHandleTracer::getInstance()->traceBrush(rv);
    return rv;
}

HBRUSH __stdcall Detour_CreatePatternBrush(HBITMAP a0)
{
    auto rv = API_CreatePatternBrush(a0);
    GdiHandleTracer::getInstance()->traceBrush(rv);
    return rv;
}

HBRUSH __stdcall Detour_CreateSolidBrush(COLORREF a0)
{
    auto rv = API_CreateSolidBrush(a0);
    GdiHandleTracer::getInstance()->traceBrush(rv);
    return rv;
}

// Palette API detour functions.

HPALETTE __stdcall Detour_CreateHalftonePalette(HDC a0)
{
    auto rv = API_CreateHalftonePalette(a0);
    GdiHandleTracer::getInstance()->tracePalette(rv);
    return rv;
}

HPALETTE __stdcall Detour_CreatePalette(LOGPALETTE* a0)
{
    auto rv = API_CreatePalette(a0);
    GdiHandleTracer::getInstance()->tracePalette(rv);
    return rv;
}

// Device context API detour functions.

HDC __stdcall Detour_CreateCompatibleDC(HDC a0)
{
    auto rv = API_CreateCompatibleDC(a0);
    GdiHandleTracer::getInstance()->traceDc(rv);
    return rv;
}

HDC __stdcall Detour_CreateDCA(LPCSTR a0,
    LPCSTR a1,
    LPCSTR a2,
    CONST DEVMODEA* a3)
{
    auto rv = API_CreateDCA(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceDc(rv);
    return rv;
}

HDC __stdcall Detour_CreateDCW(LPCWSTR a0,
    LPCWSTR a1,
    LPCWSTR a2,
    CONST DEVMODEW* a3)
{
    auto rv = API_CreateDCW(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceDc(rv);
    return rv;
}

HDC __stdcall Detour_CreateICA(LPCSTR a0,
    LPCSTR a1,
    LPCSTR a2,
    CONST DEVMODEA* a3)
{
    auto rv = API_CreateICA(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceDc(rv);
    return rv;
}

HDC __stdcall Detour_CreateICW(LPCWSTR a0,
    LPCWSTR a1,
    LPCWSTR a2,
    CONST DEVMODEW* a3)
{
    auto rv = API_CreateICW(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceDc(rv);
    return rv;
}

// Font API detour functions.

HFONT __stdcall Detour_CreateFontA(int a0,
    int a1,
    int a2,
    int a3,
    int a4,
    DWORD a5,
    DWORD a6,
    DWORD a7,
    DWORD a8,
    DWORD a9,
    DWORD a10,
    DWORD a11,
    DWORD a12,
    LPCSTR a13)
{
    auto rv = API_CreateFontA(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
    GdiHandleTracer::getInstance()->traceFont(rv);
    return rv;
}

HFONT __stdcall Detour_CreateFontIndirectA(LOGFONTA* a0)
{
    auto rv = API_CreateFontIndirectA(a0);
    GdiHandleTracer::getInstance()->traceFont(rv);
    return rv;
}

HFONT __stdcall Detour_CreateFontIndirectExA(ENUMLOGFONTEXDVA* a0)
{
    auto rv = API_CreateFontIndirectExA(a0);
    GdiHandleTracer::getInstance()->traceFont(rv);
    return rv;
}

HFONT __stdcall Detour_CreateFontIndirectExW(LOGFONTA* a0)
{
    auto rv = API_CreateFontIndirectA(a0);
    GdiHandleTracer::getInstance()->traceFont(rv);
    return rv;
}

HFONT __stdcall Detour_CreateFontIndirectW(ENUMLOGFONTEXDVW* a0)
{
    auto rv = API_CreateFontIndirectExW(a0);
    GdiHandleTracer::getInstance()->traceFont(rv);
    return rv;
}

HFONT __stdcall Detour_CreateFontW(int a0,
    int a1,
    int a2,
    int a3,
    int a4,
    DWORD a5,
    DWORD a6,
    DWORD a7,
    DWORD a8,
    DWORD a9,
    DWORD a10,
    DWORD a11,
    DWORD a12,
    LPCWSTR a13)
{
    auto rv = API_CreateFontW(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
    GdiHandleTracer::getInstance()->traceFont(rv);
    return rv;
}

// Metafile API detour functions.

HDC __stdcall Detour_CreateEnhMetaFileA(HDC a0,
    LPCSTR a1,
    RECT* a2,
    LPCSTR a3)
{
    auto rv = API_CreateEnhMetaFileA(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceMetafile(rv);
    return rv;
}

HDC __stdcall Detour_CreateEnhMetaFileW(HDC a0,
    LPCWSTR a1,
    RECT* a2,
    LPCWSTR a3)
{
    auto rv = API_CreateEnhMetaFileW(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceMetafile(rv);
    return rv;
}

HDC __stdcall Detour_CreateMetaFileA(LPCSTR a0)
{
    auto rv = API_CreateMetaFileA(a0);
    GdiHandleTracer::getInstance()->traceMetafile(rv);
    return rv;
}

HDC __stdcall Detour_CreateMetaFileW(LPCWSTR a0)
{
    auto rv = API_CreateMetaFileW(a0);
    GdiHandleTracer::getInstance()->traceMetafile(rv);
    return rv;
}

// Region API detour functions.

HRGN __stdcall Detour_CreateEllipticRgn(int a0,
    int a1,
    int a2,
    int a3)
{
    auto rv = API_CreateEllipticRgn(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_CreateEllipticRgnIndirect(RECT* a0)
{
    auto rv = API_CreateEllipticRgnIndirect(a0);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_CreatePolygonRgn(POINT* a0,
    int a1,
    int a2)
{
    auto rv = API_CreatePolygonRgn(a0, a1, a2);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_CreatePolyPolygonRgn(POINT* a0,
    INT* a1,
    int a2,
    int a3)
{
    auto rv = API_CreatePolyPolygonRgn(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_CreateRectRgn(int a0,
    int a1,
    int a2,
    int a3)
{
    auto rv = API_CreateRectRgn(a0, a1, a2, a3);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_CreateRectRgnIndirect(RECT* a0)
{
    auto rv = API_CreateRectRgnIndirect(a0);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_CreateRoundRectRgn(int a0,
    int a1,
    int a2,
    int a3,
    int a4,
    int a5)
{
    auto rv = API_CreateRoundRectRgn(a0, a1, a2, a3, a4, a5);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_ExtCreateRegion(XFORM* a0,
    DWORD a1,
    RGNDATA* a2)
{
    auto rv = API_ExtCreateRegion(a0, a1, a2);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

HRGN __stdcall Detour_PathToRegion(HDC a0)
{
    auto rv = API_PathToRegion(a0);
    GdiHandleTracer::getInstance()->traceRegion(rv);
    return rv;
}

// GDI handle release API detour functions.

BOOL __stdcall Detour_DeleteDC(HDC a0)
{
    auto rv = API_DeleteDC(a0);
    GdiHandleTracer::getInstance()->removeDc(a0);
    return rv;
}

BOOL __stdcall Detour_DeleteObject(HGDIOBJ a0)
{
    auto rv = API_DeleteObject(a0);
    GdiHandleTracer::getInstance()->removeGdiObject(a0);
    return rv;
}

BOOL __stdcall Detour_DeleteMetaFile(HMETAFILE a0)
{
    auto rv = API_DeleteMetaFile(a0);
    GdiHandleTracer::getInstance()->removeMetafile((HDC)(a0));
    return rv;
}

BOOL __stdcall Detour_DeleteEnhMetaFile(HENHMETAFILE a0)
{
    auto rv = API_DeleteEnhMetaFile(a0);
    GdiHandleTracer::getInstance()->removeMetafile((HDC)(a0));
    return rv;
}

void GdiHandleTracer::checkGdiHandlesCount()
{
    auto gdiHandlesCount = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (gdiHandlesCount >= kReportTriggerGdiCount)
    {
        saveReportToFile(m_reportPath);
        m_reportCreated = true;
        clear();
    }
}

void GdiHandleTracer::clear()
{
    m_bitmapHandles.clear();
    m_brushHandles.clear();
    m_paletteHandles.clear();
    m_dcHandles.clear();
    m_fontHandles.clear();
    m_metafileHandles.clear();
    m_regionHandles.clear();
}

bool GdiHandleTracer::isReportCreated() const
{
    return m_reportCreated;
}

GdiHandleTracer::GdiHandleTracer()
{
    m_callStackProvider.reset(new CallStackProvider);
}

GdiHandleTracer* GdiHandleTracer::getInstance()
{
    if (!m_instance)
        m_instance = new GdiHandleTracer();
    return m_instance;
}

void GdiHandleTracer::attachGdiDetours()
{
    DetourTransactionBegin();

    DetourUpdateThread(GetCurrentThread());

    DetourAttach(&(PVOID&)API_CreateBitmap, Detour_CreateBitmap);
    DetourAttach(&(PVOID&)API_CreateBitmapIndirect, Detour_CreateBitmapIndirect);
    DetourAttach(&(PVOID&)API_CreateCompatibleBitmap, Detour_CreateCompatibleBitmap);
    DetourAttach(&(PVOID&)API_CreateDIBSection, Detour_CreateDIBSection);
    DetourAttach(&(PVOID&)API_CreateDIBitmap, Detour_CreateDIBitmap);
    DetourAttach(&(PVOID&)API_CreateDiscardableBitmap, Detour_CreateDiscardableBitmap);

    DetourAttach(&(PVOID&)API_CreateBrushIndirect, Detour_CreateBrushIndirect);
    DetourAttach(&(PVOID&)API_CreateDIBPatternBrush, Detour_CreateDIBPatternBrush);
    DetourAttach(&(PVOID&)API_CreateDIBPatternBrushPt, Detour_CreateDIBPatternBrushPt);
    DetourAttach(&(PVOID&)API_CreateHatchBrush, Detour_CreateHatchBrush);
    DetourAttach(&(PVOID&)API_CreatePatternBrush, Detour_CreatePatternBrush);
    DetourAttach(&(PVOID&)API_CreateSolidBrush, Detour_CreateSolidBrush);

    DetourAttach(&(PVOID&)API_CreateHalftonePalette, Detour_CreateHalftonePalette);
    DetourAttach(&(PVOID&)API_CreatePalette, Detour_CreatePalette);

    DetourAttach(&(PVOID&)API_CreateCompatibleDC, Detour_CreateCompatibleDC);
    DetourAttach(&(PVOID&)API_CreateDCA, Detour_CreateDCA);
    DetourAttach(&(PVOID&)API_CreateDCW, Detour_CreateDCW);
    DetourAttach(&(PVOID&)API_CreateICA, Detour_CreateICA);
    DetourAttach(&(PVOID&)API_CreateICW, Detour_CreateICW);

    DetourAttach(&(PVOID&)API_CreateFontA, Detour_CreateFontA);
    DetourAttach(&(PVOID&)API_CreateFontIndirectA, Detour_CreateFontIndirectA);
    DetourAttach(&(PVOID&)API_CreateFontIndirectExA, Detour_CreateFontIndirectExA);
    DetourAttach(&(PVOID&)API_CreateFontIndirectExW, Detour_CreateFontIndirectExW);
    DetourAttach(&(PVOID&)API_CreateFontIndirectW, Detour_CreateFontIndirectW);
    DetourAttach(&(PVOID&)API_CreateFontW, Detour_CreateFontW);

    DetourAttach(&(PVOID&)API_CreateEnhMetaFileA, Detour_CreateEnhMetaFileA);
    DetourAttach(&(PVOID&)API_CreateEnhMetaFileW, Detour_CreateEnhMetaFileW);
    DetourAttach(&(PVOID&)API_CreateMetaFileA, Detour_CreateMetaFileA);
    DetourAttach(&(PVOID&)API_CreateMetaFileW, Detour_CreateMetaFileW);

    DetourAttach(&(PVOID&)API_CreateEllipticRgn, Detour_CreateEllipticRgn);
    DetourAttach(&(PVOID&)API_CreateEllipticRgnIndirect, Detour_CreateEllipticRgnIndirect);
    DetourAttach(&(PVOID&)API_CreatePolyPolygonRgn, Detour_CreatePolyPolygonRgn);
    DetourAttach(&(PVOID&)API_CreatePolygonRgn, Detour_CreatePolygonRgn);
    DetourAttach(&(PVOID&)API_CreateRectRgn, Detour_CreateRectRgn);
    DetourAttach(&(PVOID&)API_CreateRectRgnIndirect, Detour_CreateRectRgnIndirect);
    DetourAttach(&(PVOID&)API_CreateRoundRectRgn, Detour_CreateRoundRectRgn);
    DetourAttach(&(PVOID&)API_ExtCreateRegion, Detour_ExtCreateRegion);
    DetourAttach(&(PVOID&)API_PathToRegion, Detour_PathToRegion);

    DetourAttach(&(PVOID&)API_DeleteDC, Detour_DeleteDC);
    DetourAttach(&(PVOID&)API_DeleteObject, Detour_DeleteObject);
    DetourAttach(&(PVOID&)API_DeleteMetaFile, Detour_DeleteMetaFile);
    DetourAttach(&(PVOID&)API_DeleteEnhMetaFile, Detour_DeleteEnhMetaFile);

    DetourTransactionCommit();
}

void GdiHandleTracer::detachGdiDetours()
{
    DetourTransactionBegin();

    DetourUpdateThread(GetCurrentThread());

    DetourDetach(&(PVOID&)API_CreateBitmap, Detour_CreateBitmap);
    DetourDetach(&(PVOID&)API_CreateBitmapIndirect, Detour_CreateBitmapIndirect);
    DetourDetach(&(PVOID&)API_CreateCompatibleBitmap, Detour_CreateCompatibleBitmap);
    DetourDetach(&(PVOID&)API_CreateDIBSection, Detour_CreateDIBSection);
    DetourDetach(&(PVOID&)API_CreateDIBitmap, Detour_CreateDIBitmap);
    DetourDetach(&(PVOID&)API_CreateDiscardableBitmap, Detour_CreateDiscardableBitmap);

    DetourDetach(&(PVOID&)API_CreateBrushIndirect, Detour_CreateBrushIndirect);
    DetourDetach(&(PVOID&)API_CreateDIBPatternBrush, Detour_CreateDIBPatternBrush);
    DetourDetach(&(PVOID&)API_CreateDIBPatternBrushPt, Detour_CreateDIBPatternBrushPt);
    DetourDetach(&(PVOID&)API_CreateHatchBrush, Detour_CreateHatchBrush);
    DetourDetach(&(PVOID&)API_CreatePatternBrush, Detour_CreatePatternBrush);
    DetourDetach(&(PVOID&)API_CreateSolidBrush, Detour_CreateSolidBrush);

    DetourDetach(&(PVOID&)API_CreateHalftonePalette, Detour_CreateHalftonePalette);
    DetourDetach(&(PVOID&)API_CreatePalette, Detour_CreatePalette);

    DetourDetach(&(PVOID&)API_CreateCompatibleDC, Detour_CreateCompatibleDC);
    DetourDetach(&(PVOID&)API_CreateDCA, Detour_CreateDCA);
    DetourDetach(&(PVOID&)API_CreateDCW, Detour_CreateDCW);
    DetourDetach(&(PVOID&)API_CreateICA, Detour_CreateICA);
    DetourDetach(&(PVOID&)API_CreateICW, Detour_CreateICW);

    DetourDetach(&(PVOID&)API_CreateFontA, Detour_CreateFontA);
    DetourDetach(&(PVOID&)API_CreateFontIndirectA, Detour_CreateFontIndirectA);
    DetourDetach(&(PVOID&)API_CreateFontIndirectExA, Detour_CreateFontIndirectExA);
    DetourDetach(&(PVOID&)API_CreateFontIndirectExW, Detour_CreateFontIndirectExW);
    DetourDetach(&(PVOID&)API_CreateFontIndirectW, Detour_CreateFontIndirectW);
    DetourDetach(&(PVOID&)API_CreateFontW, Detour_CreateFontW);

    DetourDetach(&(PVOID&)API_CreateEnhMetaFileA, Detour_CreateEnhMetaFileA);
    DetourDetach(&(PVOID&)API_CreateEnhMetaFileW, Detour_CreateEnhMetaFileW);
    DetourDetach(&(PVOID&)API_CreateMetaFileA, Detour_CreateMetaFileA);
    DetourDetach(&(PVOID&)API_CreateMetaFileW, Detour_CreateMetaFileW);

    DetourDetach(&(PVOID&)API_CreateEllipticRgn, Detour_CreateEllipticRgn);
    DetourDetach(&(PVOID&)API_CreateEllipticRgnIndirect, Detour_CreateEllipticRgnIndirect);
    DetourDetach(&(PVOID&)API_CreatePolyPolygonRgn, Detour_CreatePolyPolygonRgn);
    DetourDetach(&(PVOID&)API_CreatePolygonRgn, Detour_CreatePolygonRgn);
    DetourDetach(&(PVOID&)API_CreateRectRgn, Detour_CreateRectRgn);
    DetourDetach(&(PVOID&)API_CreateRectRgnIndirect, Detour_CreateRectRgnIndirect);
    DetourDetach(&(PVOID&)API_CreateRoundRectRgn, Detour_CreateRoundRectRgn);
    DetourDetach(&(PVOID&)API_ExtCreateRegion, Detour_ExtCreateRegion);
    DetourDetach(&(PVOID&)API_PathToRegion, Detour_PathToRegion);

    DetourDetach(&(PVOID&)API_DeleteDC, Detour_DeleteDC);
    DetourDetach(&(PVOID&)API_DeleteObject, Detour_DeleteObject);
    DetourDetach(&(PVOID&)API_DeleteMetaFile, Detour_DeleteMetaFile);
    DetourDetach(&(PVOID&)API_DeleteEnhMetaFile, Detour_DeleteEnhMetaFile);

    DetourTransactionCommit();
}

void GdiHandleTracer::setReportPath(const std::filesystem::path& path)
{
    m_reportPath = path;
}

std::string GdiHandleTracer::getReport() const
{
    std::stringstream stream;

    auto outputCallStack =
        [&stream](const std::string& handleTypeStr, HGDIOBJ handle, const std::string& callStack)
        {
            if (callStack.find("CreateIconIndirect") != std::string::npos)
                return;

            stream << handleTypeStr << ": 0x";
            stream << std::hex << std::setw(16) << std::right << std::setfill('0');
            stream << (unsigned long long)handle << std::endl;
            stream << callStack << std::endl;
            stream << std::endl;
        };

    for (auto i = m_bitmapHandles.cbegin(); i != m_bitmapHandles.cend(); ++i)
        outputCallStack("HBITMAP", static_cast<HGDIOBJ>(i->first), i->second);

    for (auto i = m_brushHandles.cbegin(); i != m_brushHandles.cend(); ++i)
        outputCallStack("HBRUSH", static_cast<HGDIOBJ>(i->first), i->second);

    for (auto i = m_paletteHandles.cbegin(); i != m_paletteHandles.cend(); ++i)
        outputCallStack("HPALETTE", static_cast<HGDIOBJ>(i->first), i->second);

    for (auto i = m_dcHandles.cbegin(); i != m_dcHandles.cend(); ++i)
        outputCallStack("HDC", static_cast<HGDIOBJ>(i->first), i->second);

    for (auto i = m_fontHandles.cbegin(); i != m_fontHandles.cend(); ++i)
        outputCallStack("HFONT", static_cast<HGDIOBJ>(i->first), i->second);

    for (auto i = m_metafileHandles.cbegin(); i != m_metafileHandles.cend(); ++i)
        outputCallStack("HDC", static_cast<HGDIOBJ>(i->first), i->second);

    for (auto i = m_regionHandles.cbegin(); i != m_regionHandles.cend(); ++i)
        outputCallStack("HRGN", static_cast<HGDIOBJ>(i->first), i->second);

    return stream.str();
}

void GdiHandleTracer::saveReportToFile(const std::filesystem::path& path) const
{
    if (path.empty())
        return;
    std::ofstream outFileStream(path, std::ofstream::out | std::ofstream::trunc);
    if (outFileStream.is_open())
    {
        outFileStream << getReport();
        outFileStream.close();
    }
}

void GdiHandleTracer::traceBitmap(HBITMAP handle)
{
    if (!isReportCreated())
    {
        m_bitmapHandles[handle] = m_callStackProvider->getCallStack(kTopEntriesStripCount);
        checkGdiHandlesCount();
    }
}

void GdiHandleTracer::traceBrush(HBRUSH handle)
{
    if (!isReportCreated())
    {
        m_brushHandles[handle] = m_callStackProvider->getCallStack(kTopEntriesStripCount);
        checkGdiHandlesCount();
    }
}

void GdiHandleTracer::tracePalette(HPALETTE handle)
{
    if (!isReportCreated())
    {
        m_paletteHandles[handle] = m_callStackProvider->getCallStack(kTopEntriesStripCount);
        checkGdiHandlesCount();
    }
}

void GdiHandleTracer::traceFont(HFONT handle)
{
    if (!isReportCreated())
    {
        m_fontHandles[handle] = m_callStackProvider->getCallStack(kTopEntriesStripCount);
        checkGdiHandlesCount();
    }
}

void GdiHandleTracer::traceRegion(HRGN handle)
{
    if (!isReportCreated())
    {
        m_regionHandles[handle] = m_callStackProvider->getCallStack(kTopEntriesStripCount);
        checkGdiHandlesCount();
    }
}

void GdiHandleTracer::removeGdiObject(HGDIOBJ handle)
{
    if (!isReportCreated())
    {
        m_bitmapHandles.erase(static_cast<HBITMAP>(handle));
        m_brushHandles.erase(static_cast<HBRUSH>(handle));
        m_paletteHandles.erase(static_cast<HPALETTE>(handle));
        m_dcHandles.erase(static_cast<HDC>(handle));
        m_fontHandles.erase(static_cast<HFONT>(handle));
        m_regionHandles.erase(static_cast<HRGN>(handle));
    }
}

void GdiHandleTracer::traceMetafile(HDC handle)
{
    if (!isReportCreated())
    {
        m_metafileHandles[handle] = m_callStackProvider->getCallStack(kTopEntriesStripCount);
        checkGdiHandlesCount();
    }
}

void GdiHandleTracer::removeMetafile(HDC handle)
{
    if (!isReportCreated())
        m_metafileHandles.erase(handle);
}

void GdiHandleTracer::traceDc(HDC handle)
{
    if (!isReportCreated())
    {
        m_dcHandles[handle] = m_callStackProvider->getCallStack(kTopEntriesStripCount);
        checkGdiHandlesCount();
    }
}

void GdiHandleTracer::removeDc(HDC handle)
{
    if (!isReportCreated())
        m_dcHandles.erase(handle);
}

} // namespace gdi_detours
} // namespace nx
