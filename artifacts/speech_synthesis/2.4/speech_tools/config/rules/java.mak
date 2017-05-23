 ###########################################################################
 ##                                                                       ##
 ##                Centre for Speech Technology Research                  ##
 ##                     University of Edinburgh, UK                       ##
 ##                         Copyright (c) 1996                            ##
 ##                        All Rights Reserved.                           ##
 ##                                                                       ##
 ##  Permission is hereby granted, free of charge, to use and distribute  ##
 ##  this software and its documentation without restriction, including   ##
 ##  without limitation the rights to use, copy, modify, merge, publish,  ##
 ##  distribute, sublicense, and/or sell copies of this work, and to      ##
 ##  permit persons to whom this work is furnished to do so, subject to   ##
 ##  the following conditions:                                            ##
 ##   1. The code must retain the above copyright notice, this list of    ##
 ##      conditions and the following disclaimer.                         ##
 ##   2. Any modifications must be clearly marked as such.                ##
 ##   3. Original authors' names are not deleted.                         ##
 ##   4. The authors' names are not used to endorse or promote products   ##
 ##      derived from this software without specific prior written        ##
 ##      permission.                                                      ##
 ##                                                                       ##
 ##  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ##
 ##  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
 ##  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
 ##  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ##
 ##  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
 ##  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
 ##  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
 ##  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
 ##  THIS SOFTWARE.                                                       ##
 ##                                                                       ##
 ###########################################################################
 ##                                                                       ##
 ##                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             ##
 ##                   Date: Fri Feb 27th 1998                             ##
 ## --------------------------------------------------------------------  ##
 ## Rules for java.                                                       ##
 ##                                                                       ##
 ###########################################################################

ifndef JAVA_TOP
    JAVA_TOP     = $(PROJECT_JAVA_ROOT)
endif
JAVA_PACKAGE = $(shell echo $(DIRNAME)| sed -e 's%.*/\($(JAVA_TOP_PACK)/.*\)%\1%' -e s%/%.%g)

JAVA_CLASSES_CLASS = $(JAVA_CLASSES:%=%.class)
JAVA_CLASSES_JAVA = $(JAVA_CLASSES:%=%.java)

java_root.mak: FORCE
	@{ \
	echo 'LOCAL_JAVA_CLASS_LIBRARY=$(TOP)/lib/$(JAVA_CODE_VERSION).jar' ;\
	echo 'LOCAL_JAVA_CLASSPATH=$($(PROJECT_PREFIX)_HOME)/lib/$(JAVA_CODE_VERSION).jar' ;\
	echo 'LOCAL_JAVA_ROOT=$(DIRNAME)' ;\
	} > java_root.mak

ifdef JAVA_NATIVE_CLASSES
    JAVA_CLASSES_O = $(JAVA_NATIVE_CLASSES:%=%.o)

jni_%.h : %.class
	@$(RM) -f jni_$*.h
	$(JAVAH) -classpath '$(PROJECT_JAVA_ROOT):$(JAVA_CLASSPATH)' -o jni_$*.h $(JAVA_PACKAGE).$*

INCLUDES += $(JAVA_INCLUDES)

.java : $(JAVA_CLASSES_CLASS) $(JAVA_NATIVE_CLASSES:%=jni_%.h) 

else

.java :  $(JAVA_CLASSES_CLASS)

endif

.javalib:  FORCE
	@if [ -f $(JAVA_TOP)/.java_updates ] ;\
		then \
		$(MAKE) .java_class_list ;\
		echo Create Java Library $(JAVA_CLASS_LIBRARY) ;\
		$(JAR) $(JAVA_CLASS_LIBRARY) `cat .java_class_list` ;\
		$(RM) $(JAVA_TOP)/.java_updates ;\
	fi


.java_class_list: FORCE
	@echo Finding classes
	@find . -follow \( -name RCS -prune \) -o -name '*.class' -print | sed -e '/^\.\//s///' > .java_class_list

ifndef JAVAC_COMMAND
    JAVAC_COMMAND = $(JAVAC) $(JAVAFLAGS) -classpath '$(PROJECT_JAVA_ROOT):$(JAVA_CLASSPATH)'
endif

%.class : %.java
	$(strip $(JAVAC_COMMAND)  $*.java)
	@$(ECHO_N) $*.class >> $(JAVA_TOP)/.java_updates


