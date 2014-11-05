import os, sys, subprocess, shutil
from subprocess import Popen, PIPE

beta_suffix = '-beta' if '${beta}' == 'true' else ''

server_msi_folder = '..\\..\\wixsetup-server-only\\${arch}\\bin'
client_msi_folder = '..\\..\\wixsetup-client-only\\${arch}\\bin'

server_msi_name = '${namespace.minor}-${namespace.additional}-server-${release.version}.${buildNumber}-${box}-${arch}-${build.configuration}%s.msi' % beta_suffix
client_msi_name = '${namespace.minor}-${namespace.additional}-client-${release.version}.${buildNumber}-${box}-${arch}-${build.configuration}%s.msi' % beta_suffix

client_msi_product_code = subprocess.check_output('cscript //NoLogo productcode.js %s\\%s' % (client_msi_folder, client_msi_name)).strip()
server_msi_product_code = subprocess.check_output('cscript //NoLogo productcode.js %s\\%s' % (server_msi_folder, server_msi_name)).strip()

with open('generated_variables.iss', 'w') as f:
    print >> f, '#define ServerMsiFolder "%s"' % server_msi_folder
    print >> f, '#define ClientMsiFolder "%s"' % client_msi_folder
    print >> f, '#define ServerMsiName "%s"' % server_msi_name
    print >> f, '#define ClientMsiName "%s"' % client_msi_name
    print >> f, '#define ServerMsiProductCode "%s"' % server_msi_product_code
    print >> f, '#define ClientMsiProductCode "%s"' % client_msi_product_code

commands = [
    os.path.join(os.getenv('environment'), "is5/ISCC.exe") + ' /obin /f${project.build.finalName} Product-full.iss'
]

for command in commands:
    p = subprocess.Popen(command, shell=True, stdout=PIPE)
    out, err = p.communicate()
    print ('\n++++++++++++++++++++++%s++++++++++++++++++++++' % command)
    print out
    p.wait()
    if p.returncode != 0 and p.returncode != 204:  
        print "failed with code: %s" % str(p.returncode) 
        sys.exit(1)