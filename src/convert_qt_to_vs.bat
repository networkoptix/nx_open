qmake -tp vc -o %1.vcproj %1.pro
qmake_vc_fixer %1.vcproj
del %1.vcproj
rename %1.new.vcproj %1.vcproj
