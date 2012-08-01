#ifndef onvif_resource_settings_h
#define onvif_resource_settings_h

struct WhiteBalance
{
    bool manual;
    float rGain;
    float bGain;

    WhiteBalance() :
        manual(false),
        rGain(-1),
        bGain(-1)
    {}
};

struct Exposure
{
    bool manual;
    bool lowNoisePriority;
    float minExposureTime;
    float maxExposureTime;
    float minGain;
    float maxGain;
    float minIris;
    float maxIris;
    float exposureTime;
    float gain;
    float iris;

    Exposure() :
        manual(false),
        lowNoisePriority(false),
        minExposureTime(-1),
        maxExposureTime(-1),
        minGain(-1),
        maxGain(-1),
        minIris(-1),
        maxIris(-1),
        exposureTime(-1),
        gain(-1),
        iris(-1)
    {}
};

class QnOnvifResourceSettings
{
    float m_brightness;
    float m_sharpness;
    float m_colorSaturation;
    WhiteBalance m_whiteBalance;
    Exposure m_exposure;

public:

    QnOnvifResourceSettings();

    ~QnOnvifResourceSettings();

    //Adjustment
    bool setBrightness(float val);
    float getBrightness();

    bool setSharpness(float val);
    float getSharpness();

    bool setColorSaturation(float val);
    float getColorSaturation();

    //Colour
    bool setWhiteBalance(WhiteBalance balance);
    WhiteBalance setWhiteBalance();

    //Exposure
    bool setExposure(Exposure exp);
    Exposure setExposure();

    //Administration
    bool reboot();
    bool softReset();
    bool hardReset();

private:

    QnOnvifResourceSettings(const QnOnvifResourceSettings&);
};

#endif //onvif_resource_settings_h
