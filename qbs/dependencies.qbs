import qbs 1.0
import qbs.Environment
import qbs.FileInfo
import qbs.File
import qbs.TextFile

import Rdep

Project
{
    RdepProbe
    {
        id: boost
        packageName: "boost"
        target: "any"
    }

    RdepProbe
    {
        id: qtservice
        packageName: "qtservice"
        target: "any"
    }
    RdepProbe
    {
        id: qtsinglecoreapplication
        packageName: "qtsinglecoreapplication"
        target: "any"
    }
    RdepProbe
    {
        id: qtsingleguiapplication
        condition: withMobileClient
        packageName: "qtsingleguiapplication"
        target: "any"
    }
    RdepProbe
    {
        id: qtsingleapplication
        condition: withDesktopClient
        packageName: "qtsingleapplication"
        target: "any"
    }

    RdepProbe
    {
        id: openssl
        packageName: "openssl"
    }

    RdepProbe
    {
        id: ffmpeg
        packageName: "ffmpeg"
    }

    RdepProbe
    {
        id: quazip
        packageName: "quazip"
        condition: withDesktopClient || withMediaServer
    }

    RdepProbe
    {
        id: openal
        packageName: "openal"
        condition: (withDesktopClient || withMobileClient)
            && (qbs.targetOS.contains("windows") || qbs.targetOS.contains("android"))
    }

    RdepProbe
    {
        id: festival
        condition: withDesktopClient || withMediaServer
        packageName: "festival"
    }

    RdepProbe
    {
        id: festivalVox
        condition: withDesktopClient || withMediaServer
        packageName: "festival-vox"
        target: "any"
    }

    RdepProbe
    {
        id: robotoFont
        condition: withDesktopClient || withMobileClient
        packageName: "roboto-ttf"
        target: "any"
    }

    RdepProbe
    {
        id: robotoMonoFont
        condition: withDesktopClient || withMobileClient
        packageName: "roboto-mono-ttf"
        target: "any"
    }

    references:
    {
        var result = [
            boost.packagePath,
            openssl.packagePath,
            qtservice.packagePath,
            qtsinglecoreapplication.packagePath
        ]

        qtsingleapplication.found && result.push(qtsingleapplication.packagePath)
        qtsingleguiapplication.found && result.push(qtsingleguiapplication.packagePath)

        ffmpeg.found && result.push(ffmpeg.packagePath)
        quazip.found && result.push(quazip.packagePath)
        openal.found && result.push(openal.packagePath)

        festival.found && result.push(festival.packagePath)
        festivalVox.found && result.push(festivalVox.packagePath)

        robotoFont.found && result.push(robotoFont.packagePath)
        robotoMonoFont.found && result.push(robotoMonoFont.packagePath)

        return result
    }
}
