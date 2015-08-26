
#pragma once

struct QnStartupParameters
{
    enum { kInvalidScreen = -1 };

    static QnStartupParameters fromCommandLineArg(int argc
        , char **argv);

    static QnStartupParameters createDefault();

    int screen;

    bool noSingleApplication;
    bool skipMediaFolderScan;
    bool noVersionMismatchCheck;
    bool noVSync;
    bool noClientUpdate;
    bool softwareYuv;
    bool forceLocalSettings;
    bool noFullScreen;

    QString devModeKey;
    QString authenticationString;
    QString delayedDrop;
    QString instantDrop;
    QString logLevel;
    QString ec2TranLogLevel;
    QString translationPath;
    QString lightMode;
    QString sVideoWallGuid;
    QString sVideoWallItemGuid;
    QString engineVersion;
    QString dynamicCustomizationPath;

private:
    QnStartupParameters();
};
