import subprocess

proc = subprocess.Popen(["hg", "branch"], stdout=subprocess.PIPE, shell=False)
(out, err) = proc.communicate()
print "branch:", out

f = open('.branch.properties','w')
f.write('branch=%s' % out) # python will convert \n to os.linesep
f.close() # you can omit in most cases as the destructor will call it