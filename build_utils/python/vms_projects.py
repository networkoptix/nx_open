class TranslatableProject():
    name = ""
    path = ""
    extensions = "cpp,h"
    locations = "none"

    def __init__(self, name, path = None):
        self.name = name
        self.path = path if path else name
        
    def ui(self):
        self.extensions = "ui"
        self.locations = "relative"
        return self
      
    def qml(self):
        self.extensions = "qml"
        return self
        
    def __repr__(self):
        return "<TranslatableProject name:%s path:%s extensions:%s locations:%s>" % (self.name, self.path, self.extensions, self.locations)

translatableProjects = [   
    TranslatableProject("common"),
    TranslatableProject("traytool"),
    TranslatableProject("client_base", "client/libclient"),
    TranslatableProject("client_ui", "client/libclient").ui(),
    TranslatableProject("client_core", "client/libclient_core"),
    TranslatableProject("mobile_client_base", "client/mobile_client"),
    TranslatableProject("mobile_client_qml", "client/mobile_client").qml()
]

def getTranslatableProjects():
    return translatableProjects
