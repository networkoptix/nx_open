#pragma once

#include <Windows.h>
#include <string>

class QnLauncherProgress
{
public:
    QnLauncherProgress(const std::wstring& caption);
    ~QnLauncherProgress();

    void setRange(long long minPos, long long maxPos);
    void setPos(long long pos);

private:
    void processMessages();
    void updateCaption(int value);

private:
    HWND m_mainWnd = 0;
    HWND m_progressWnd = 0;
    long long m_minPos = 0;
    double m_scale = 1.0;
    std::wstring m_caption;
};
