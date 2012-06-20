class Component:
    "Component descriptor"
    def __init__(self, name, description):
        self.name = name
        self.description = description

    def __eq__(self, other):
        return self.name == other.name

class Version:
    "Single version"
    def __init__(self, *args):
        self.version = args
        
    def __len__(self):
        return 1

    def __getitem__(self, key):
        if key == 0:
            return self.version

        raise IndexError()

    def __eq__(self, other):
        return self.version == other.version

class Range:
    "Version range"
    def __init__(self, xfrom, xto):
        self.xfrom = xfrom
        self.xto = xto

    def __len__(self):
        return self.xto.version[1] - self.xfrom.version[1] + 1

    def __getitem__(self, key):
        if 0 <= key < self.__len__():
            return (self.xfrom.version[0], self.xfrom.version[1] + key)

        raise IndexError()

class CV:
    "Component and Version"
    def __init__(self, component, version):
        self.component = component
        self.version = version

    def __eq__(self, other):
        return self.component == other.component and self.version == other.version

    def __str__(self):
        return '' + self.component + ': ' + self.version
