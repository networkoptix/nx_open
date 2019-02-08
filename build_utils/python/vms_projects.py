class CustomizableProject():

    def __init__(self, name, sources = None, static_files = None, customized_files = None, prefix = ""):
        self.name = name
        self.sources = sources
        self.static_files = static_files
        self.customized_files = customized_files if customized_files else [name]
        self.prefix = prefix
        self.ignore_parent = False

    def __repr__(self):
        return "<CustomizableProject name:%s sources:%s>" % (
            self.name, self.sources)

    def __str__(self):
        return "Project %s %s" % (
            self.name, "at {0}".format(self.sources) if self.sources else "")

    def ignoreParent(self):
        self.ignore_parent = True
        return self

#Skins will be added as separate projects - this will siplify checking logic a lot
customizableProjects = [
    CustomizableProject(
        "common",
        ["common/src"],
        None,
        ["common/resources/skin"],
        "skin"
        ),
    CustomizableProject("icons").ignoreParent(),
    CustomizableProject("mediaserver_core").ignoreParent(),
    CustomizableProject("wixsetup").ignoreParent(),
    CustomizableProject("common"),
    CustomizableProject("client-dmg"),
    CustomizableProject(
        "client",
        ["vms/client/nx_vms_client_desktop/src", "vms/client/nx_vms_client_desktop/static-resources/src"],
        ["vms/client/nx_vms_client_desktop/static-resources/skin"],
        ["vms/client/nx_vms_client_desktop/resources/skin"],
        "skin"
        ),
    CustomizableProject(
        "mobile_client",
        ["vms/client/mobile_client/src", "vms/client/mobile_client/static-resources/qml"],
        ["vms/client/mobile_client/static-resources/images"],
        ["vms/client/mobile_client/resources/images"],
        "images"
        )
]

def getCustomizableProjects():
    return customizableProjects


if __name__ == "__main__":
    print "Customizable:"
    for p in getCustomizableProjects():
        print p
