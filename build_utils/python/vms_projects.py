class TranslatableProject():
    name = ""
    path = ""
    extensions = "cpp,h"
    locations = "none"
    sources = "src"

    def __init__(self, name, path = None):
        self.name = name
        self.path = path if path else name
        
    def ui(self):
        self.extensions = "ui"
        self.locations = "relative"
        return self
      
    def qml(self):
        self.extensions = "qml"
        self.sources = "static-resources"
        return self
        
    def __repr__(self):
        return "<TranslatableProject name:%s path:%s extensions:%s locations:%s sources:%s>" % (
            self.name, self.path, self.extensions, self.locations, self.sources)
            
    def __str__(self):
        return "Project %s (at %s): %s, %s at %s" % (
            self.name, self.path, self.extensions, self.locations, self.sources)            

translatableProjects = [   
    TranslatableProject("qt", "common"),
    TranslatableProject("qtbase", "common"),
    TranslatableProject("common"),
    TranslatableProject("traytool"),
    TranslatableProject("client_base", "client/libclient"),
    TranslatableProject("client_ui", "client/libclient").ui(),
    TranslatableProject("client_core", "client/libclient_core"),
    TranslatableProject("client_qml", "client/libclient").qml(),
    TranslatableProject("mobile_client_base", "client/mobile_client"),
    TranslatableProject("mobile_client_qml", "client/mobile_client").qml()
]

def getTranslatableProjects():
    return translatableProjects

class CustomizableProject():

    def __init__(self, name, sources, static_files, customized_files, prefix = ""):
        self.name = name
        self.sources = sources
        self.static_files = static_files
        self.customized_files = customized_files
        self.prefix = prefix
               
    def __repr__(self):
        return "<CustomizableProject name:%s sources:%s>" % (
            self.name, self.sources)
            
    def __str__(self):
        return "Project %s %s" % (
            self.name, "at {0}".format(self.sources) if self.sources else "")

#Skins will be added as separate projects - this will siplify checking logic a lot            
customizableProjects = [   
    CustomizableProject(
        "common",
        ["common/src"], 
        None, 
        ["common/resources/skin"],
        "skin"
        ),
    CustomizableProject(
        "icons", 
        None, 
        None, 
        ["icons"]
        ),
    CustomizableProject(
        "client", 
        ["client/libclient/src", "client/libclient/static-resources/src"], 
        ["client/libclient/static-resources/skin"],
        ["libclient/resources/skin"],
        "skin"
        ),
    CustomizableProject(
        "mobile_client", 
        ["client/mobile_client/src", "client/mobile_client/static-resources/qml"], 
        ["client/mobile_client/static-resources/images"],
        ["mobile_client/resources/images"],
        "images"
        )
]

def getCustomizableProjects():
    return customizableProjects
    
    
if __name__ == "__main__":
    print "Translatable:"
    for p in getTranslatableProjects():
        print p
        
    print "Customizable:"
    for p in getCustomizableProjects():
        print p
