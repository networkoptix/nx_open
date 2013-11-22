class Component:
    "Component descriptor"
    def __init__(self, name, description):
        self.name = name
        self.description = description

    def __eq__(self, other):
        return self.name == other.name

class Version:
    "Single version"
    VERSIONS = []
    def __init__(self, *args):
        self.version = args
        Version.VERSIONS.append(self)
        Version.VERSIONS.sort()
        
    def __len__(self):
        return 1

    def __getitem__(self, key):
        if key == 0:
            return self

        raise IndexError()

    def __eq__(self, other):
        return self.version == other.version

    def __repr__(self):
        return str(self.version)

class Range:
    "Version range"
    def __init__(self, xfrom, xto):
        self.xfrom = xfrom
        self.xto = xto

    def __len__(self):
        return Version.VERSIONS.index(self.xto) - Version.VERSIONS.index(self.xfrom) + 1

    def __getitem__(self, key):
        if 0 <= key < self.__len__():
            return Version.VERSIONS[Version.VERSIONS.index(self.xfrom) + key]

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
