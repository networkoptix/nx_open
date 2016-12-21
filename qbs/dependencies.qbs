import qbs 1.0
import qbs.Environment
import qbs.FileInfo
import qbs.File
import qbs.TextFile

import Rdep

Project
{
    id: deps
    name: "deps"

    property string packagesDir:
        FileInfo.joinPaths(Environment.getEnv("environment"), "packages")
    property string python: "python2"

    property string boostVersion: "1.60.0"
    property string opensslVersion: "1.0.2e"
    property string ffmpegVersion: "3.1.1"
    property string quazipVersion: "0.7.1"
    property string onvifVersion: "2.1.2-io2"
    property string sigarVersion: "1.7"
    property string openldapVersion: "2.4.42"
    property string saslVersion: "2.1.26"
    property string openalVersion: "1.16"
    property string libjpeg_turboVersion: "1.4.2"
    property string libcreateprocessVersion: "0.1"
    property string festivalVersion: "2.1"
    property string festival_voxVersion: festivalVersion
    property string vmuxVersion: "1.0.0"
    property string gtestVersion: "0.7.1"
    property string gmockVersion: "0.7.1"
    property string helpVersion: customization + "-" + releaseVersion

    Probe
    {
        id: branchProbe

        property string branch

        property string _branchFile: FileInfo.joinPaths(sourceDirectory, ".hg/branch")
        property var _lastModified: File.lastModified(_branchFile)

        configure:
        {
            branch = (new TextFile(_branchFile)).readAll().trim()
        }
    }

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
        packageName: "qtsingleguiapplication"
        target: "any"
    }
    RdepProbe
    {
        id: qtsingleapplication
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
    }

    RdepProbe
    {
        id: sigar
        packageName: "sigar"
    }

    RdepProbe
    {
        id: onvif
        packageName: "onvif"
    }

    RdepProbe
    {
        id: festival
        packageName: "festival"
    }

    RdepProbe
    {
        id: festivalVox
        packageName: "festival-vox"
        target: "any"
    }

    RdepProbe
    {
        id: robotoFont
        packageName: "roboto-ttf"
        target: "any"
    }

    RdepProbe
    {
        id: robotoMonoFont
        packageName: "roboto-mono-ttf"
        target: "any"
    }

    RdepProbe
    {
        id: help
        packageName: "help"
        target: "any"
    }

    RdepProbe
    {
        id: nxSdk
        packageName: "nx_sdk-1.6.0"
        target: "any"
    }

    RdepProbe
    {
        id: nxStorageSdk
        packageName: "nx_storage_sdk-1.6.0"
        target: "any"
    }

    RdepProbe
    {
        id: serverExternalForBranch
        packageName: "server-external-" + branchProbe.branch
        target: "any"
        required: false
    }

    RdepProbe
    {
        id: serverExternal
        packageName: "server-external-" + releaseVersion
        target: "any"
        condition: !serverExternalForBranch.found
    }

    references: [
        boost.packagePath,
        qtservice.packagePath,
        qtsinglecoreapplication.packagePath,
        qtsingleguiapplication.packagePath,
        qtsingleapplication.packagePath,
        openssl.packagePath,
        ffmpeg.packagePath,
        quazip.packagePath,
        sigar.packagePath,
        onvif.packagePath,
        festival.packagePath,
        festivalVox.packagePath,
        robotoFont.packagePath,
        robotoMonoFont.packagePath,
        help.packagePath,
        nxSdk.packagePath,
        nxStorageSdk.packagePath,
        serverExternalForBranch.found
            ? serverExternalForBranch.packagePath
            : serverExternal.packagePath
    ]
}
