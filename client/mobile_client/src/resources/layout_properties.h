#pragma once

class QnLayoutProperties
{
    Q_GADGET

public:
    enum class DisplayMode
    {
        SingleCamera,
        MultipleCameras
    };
    Q_ENUM(DisplayMode)

    static const QString kDisplayMode;
    static const QString kSingleCamera;
};
