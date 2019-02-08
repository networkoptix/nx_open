class TranslatableProject():
    name = ""
    path = ""
    extensions = "cpp,h"
    locations = "none"
    sources = "src"
    groups = []

    def __init__(self, name, path=None, groups=[]):
        self.name = name
        self.path = path if path else name
        self.groups = groups

    def ui(self):
        self.extensions = "ui"
        self.locations = "relative"
        return self

    def qml(self):
        self.extensions = "qml,js"
        self.sources = "static-resources"
        return self

    def __repr__(self):
        return "<TranslatableProject name:%s path:%s extensions:%s locations:%s sources:%s>" % (
            self.name, self.path, self.extensions, self.locations, self.sources)

    def __str__(self):
        return "Project %s (at %s): %s, %s at %s" % (
            self.name, self.path, self.extensions, self.locations, self.sources)


class ProjectGroups:
    ALL = "all"
    MOBILE = "mobile"
    VMS = "vms"


TRANSLATABLE_PROJECTS = [
    TranslatableProject("vms/libs/common", groups=[ProjectGroups.MOBILE, ProjectGroups.VMS]),
    TranslatableProject("vms/traytool", groups=[ProjectGroups.VMS]),
    TranslatableProject(
        "client_base", "vms/client/nx_vms_client_desktop", groups=[ProjectGroups.VMS]),
    TranslatableProject(
        "client_ui", "vms/client/nx_vms_client_desktop", groups=[ProjectGroups.VMS]).ui(),
    TranslatableProject(
        "client_core", "vms/client/nx_vms_client_core", groups=[ProjectGroups.MOBILE, ProjectGroups.VMS]),
    TranslatableProject(
        "client_qml", "vms/client/nx_vms_client_desktop", groups=[ProjectGroups.VMS]).qml(),
    TranslatableProject(
        "mobile_client_base", "vms/client/mobile_client", groups=[ProjectGroups.MOBILE]),
    TranslatableProject(
        "mobile_client_qml", "vms/client/mobile_client", groups=[ProjectGroups.MOBILE]).qml()
]


def get_translatable_projects(group=None):
    allow_all = not group or group == ProjectGroups.ALL
    for project in TRANSLATABLE_PROJECTS:
        if allow_all or group in project.groups:
            yield project
