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

    property bool withInstallers: false

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
