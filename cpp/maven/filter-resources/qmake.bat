@echo off
set PRO-FILE=%1 
rem IF ["${force_x86}"] == ["true"] (set ARCH=x86) ELSE (set ARCH=${arch})

call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat" ${arch}
call ${qt.dir}/bin/qmake -spec ${qt.spec} -tp vc -o ${project.build.sourceDirectory}/${project.artifactId}-${arch}.vcxproj %PRO-FILE%