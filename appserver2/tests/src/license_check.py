import urllib2
import sys
import ConfigParser
import json

class LicenseCheck:

    def _load_config(self):
        cfg = ConfigParser.RawConfigParser()
        cfg.read("config.cfg")

        # get the configuration
        self._server_list = cfg.get("LicenseActivate","serverList").split('\n')
        self._license_file= cfg.get("LicenseActivate","licenseFile")
        self._log_name = cfg.get("LicenseActivate","logFile")
        
        # user name and password 
        uname = cfg.get("LicenseActivate","username")
        pwd = cfg.get("LicenseActivate","password")

        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for s in self._server_list:
            passman.add_password( "NetworkOptix", 
                                  "http://{server}/api/".format(server=s),uname, pwd )
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    def __init__(self):
        self._load_config()

    def check(self):
        with open(self._license_file,"r") as f:
            with open(self._log_name,"w+") as log:
                lines = f.read().splitlines()
                for line in lines:
                    for s in self._server_list:
                        request = urllib2.urlopen(
                            "https://{server}/api/activateLicense?key={seria_num}".\
                                format(server=s,seria_num=line))
                         
                        reply = json.loads(request.read())

                        if "error" in reply:
                            if reply["error"] != "0" :
                                log.write("SeriaNum:{seria_num}\n".format(seria_num=line))
                                log.write("Server:{server}\n".format(server=s))
                                if "errorString" in reply:
                                    log.write("ErrorString:{error}\n".format(error=reply["errorString"]))
                                else:
                                    log.write("ErrorString is not shown,error code is:{ec}\n".format(ec=reply["error"]))
                                log.write("=======================================================\n")
                                log.flush()
                        request.close()

if __name__ == "__main__":
    LicenseCheck().check()


