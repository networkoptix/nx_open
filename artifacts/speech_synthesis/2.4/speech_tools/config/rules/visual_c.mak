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
 ##                   Date: Wed Oct  1 1997                               ##
 ## --------------------------------------------------------------------  ##
 ## Support for Visual C++ by creating makefiles. This can probably be    ##
 ## modularised in some way.                                              ##
 ##                                                                       ##
 ###########################################################################

vc_make.depend: make.depend
	@echo Creating vc_make.depend for $(DIRNAME)
	@sed -e '/\.o/s//.obj/g' -e 's/[ 	]\/[^ 	]*/ /g' -e 's/\//\\/g' make.depend > vc_make.depend

ifdef VC_IGNORE
VCMakefile: FORCE
	@echo "No VC++ Support For $(DIRNAME)"
else
VCMakefile: FORCE 
	@echo Creating VCMakefile for $(DIRNAME)
	@{ \
	echo ;\
	echo '# Makefile for MicroCruft Visual C++' ;\
	echo ;\
	echo TOP=$(subst /,\\,$(TOP)) ;\
	echo ;\
	echo DIRNAME=$(subst /,\\,$(DIRNAME)) ;\
	echo ;\
	echo todo: default_target ;\
	echo ;\
	} > VCMakefile
ifdef OBJS
	@{ \
	echo OBJS = $(subst _unix,_win32,$(OBJS:%.o=%.obj));\
	echo ;\
	} >> VCMakefile
endif
ifdef AUX_OBJS
	@{ \
	echo 'AUX_OBJS = $(subst /,\,$(subst .o,.obj,$(AUX_OBJS)))' ;\
	echo ;\
	} >> VCMakefile
endif
ifdef ABSTRACT_TYPES
	@{ \
	echo 'ABSTRACT_TYPES=$(ABSTRACT_TYPES)' ;\
	echo ;\
	} >> VCMakefile
endif
ifdef BUILD_DIRS
	@{ \
	echo DIRS =  $(EXTRA_LIB_BUILD_DIRS) $(BUILD_DIRS);\
	echo ;\
	} >> VCMakefile
endif
ifdef LOCAL_INCLUDES
	@{ \
	echo LOCAL_INCLUDES = $(subst -I,/I,$(subst /,\\,$(LOCAL_INCLUDES)));\
	echo ;\
	} >> VCMakefile
endif
ifdef WIN_CFLAGS
	@{ \
	echo 'CFLAGS = $(WIN_CFLAGS)';\
	echo ;\
	} >> VCMakefile
else
	@{ \
	echo 'CFLAGS = $$(DEBUGFLAGS) $$(OPTFLAGS) $$(INCLUDEFLAGS) $(VC_LOCAL_DEFINES)' ;\
	echo ;\
	} >> VCMakefile
endif
ifdef LOCAL_DEFAULT_LIBRARY
	@echo 'INLIB = $(subst /,\,$(subst .a,.lib,$(TOP)/$$(LIB_DIR)/lib$(LOCAL_DEFAULT_LIBRARY).a))' >> VCMakefile
else
	@echo 'INLIB = $$(TOP)\$$(LIB_DIR)\$(DEFAULT_LIBRARY).lib' >> VCMakefile
endif
ifdef VCLIBS
	@echo 'VCLIBS = $(subst /,\,$(subst .a,.lib,$(VCLIBS)))' >> VCMakefile
endif
ifdef TOADD
	@echo 'TOADD = $(subst /,\,$(subst .o,.obj,$(TOADD)))' >> VCMakefile
endif
ifdef ADDLIB
	@echo 'ADDLIB = $(subst /,\,$(subst .a,.lib,$(ADDLIB)))' >> VCMakefile
endif
ifdef ALL_EXECS
	@{ \
	echo ;\
	PROG_NAMES=`echo '$(ALL_EXECS)' | sed -e 's%[^ ]*/%%g'` ;\
	echo PROGS = `echo "$$PROG_NAMES" | sed -e 's%\([^ ][^ ]*\)%\1.exe%g'` ;\
	echo ;\
	echo 'ALL = $$(PROGS)' ;\
	} >> VCMakefile
else
ifdef DOCXXFILES
	@{ \
	echo ;\
	echo ALL = .process_docs;\
	} >> VCMakefile
else
ifdef SCRIPTS
	@{ \
	echo ;\
	echo ALL = .process_scripts;\
	} >> VCMakefile
else
	@{ \
	echo ;\
	echo ALL =$(subst .o,.obj,$(subst .add_to_lib,.vc_add_to_lib, $(subst .buildlib,.vcbuildlib,$(ALL)))) ;\
	} >> VCMakefile
endif
endif
endif
	@{ \
	echo '!include $$(TOP)\config\vc_common_make_rules';\
	echo ;\
	} >> VCMakefile
ifeq (yes,$(shell test -f make.depend && echo yes))
	@$(MAKE) --no-print-directory vc_make.depend
	@{ \
	echo '!include vc_make.depend' ;\
	echo ;\
	} >> VCMakefile
endif
ifdef ALL_EXECS
	@{ \
	PROG_NAMES=`echo '$(ALL_EXECS)' | sed -e 's%[^ ]*/%%g'` ;\
	echo ;\
	for ex in $$PROG_NAMES ; do \
		if [ -f $${ex}_main.cc ] ;\
			then \
			echo "$${ex}.exe : $${ex}_main.obj  \$$(AUX_OBJS) \$$(VCLIBS)" ;\
			echo "	link/nologo \$$(LINKFLAGS) /out:$${ex}.exe $${ex}_main.obj \$$(AUX_OBJS) \$$(VCLIBS) \$$(WINLIBS)" ;\
		else \
			echo "$${ex}.exe : $${ex}.obj  \$$(AUX_OBJS) \$$(VCLIBS)" ;\
			echo "	link/nologo \$$(LINKFLAGS) /out:$${ex}.exe $${ex}.obj \$$(AUX_OBJS) \$$(VCLIBS) \$$(WINLIBS)" ;\
		fi ;\
		echo ;\
	done ;\
	echo ;\
	} >> VCMakefile
endif
	@if [ -f VCLocalRules ] ;\
		then \
		cat VCLocalRules  >> VCMakefile;\
	fi
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
	do \
	   $(MAKE) --no-print-directory -C $$i VCMakefile ; \
	done
endif

endif

VCclean:
	@echo clean in $(DIRNAME)
	$(RM) -f *.obj *.exe *.lib
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
	do \
	   $(MAKE) --no-print-directory -C $$i VCclean ; \
	done
endif


