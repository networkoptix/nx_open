import qbs
import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.TextFile
import qbs.Process
import "qbs/imports/VmsUtils/vms_utils.js" as VmsUtils

Project
{
    name: "vms"
    minimumQbsVersion: "1.8.0"
    qbsSearchPaths: ["build_utils/qbs", "qbs", "customization/" + customization + "/qbs"]

    property int versionMajor: 3
    property int versionMinor: 0
    property int versionBugfix: 0
    property int buildNumber: 0
    property string releaseVersion:
        versionMajor + "." + versionMinor + "." + versionBugfix
    property string fullVersion:
        versionMajor + "." + versionMinor + "." + versionBugfix + "." + buildNumber

    property bool developerBuild: true
    property string box
    property string customization: "default"
    property bool beta: true
    property string target: VmsUtils.currentTarget(box)
    property string platform: VmsUtils.currentPlatform()

    property bool withMediaServer:
        ["linux", "windows"].contains(platform) || box != undefined
    property bool withDesktopClient:
        ["linux", "windows", "macosx"].contains(platform) || box == "tx1"
    property bool withMobileClient:
        ["android", "ios", "linux", "windows"].contains(platform)
    property bool withClouds:
        ["linux", "windows"].contains(platform)

    property bool withInstallers: false

    property string python: "python2"
    property string packagesDir:
        FileInfo.joinPaths(Environment.getEnv("environment"), "packages")

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

    Properties
    {
        condition: qbs.targetOS.contains("macos")
        quazipVersion: "0.7.2"
        onvifVersion: "2.1.2-io"
    }

    Properties
    {
        condition: qbs.targetOS.contains("android")
        opensslVersion: "1.0.2g"
        openalVersion: "1.17.2"
    }

    property path buildenvDirectory: Environment.getEnv("environment")

    references: [
        "qbs/dependencies.qbs",
        "qbs/mercurial_info.qbs",
        "common_libs",
        "common",
        "appserver2",
        "nx_cloud",
        "client",
        "mediaserver_core",
        "mediaserver",
        "plugins",
        "debsetup"
    ]
}
