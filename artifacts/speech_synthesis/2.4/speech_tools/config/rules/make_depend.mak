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
 ## Rules to make the dependency lists for a directory.                   ##
 ##                                                                       ##
 ###########################################################################

ifndef ALL_DEPEND
    ALL_DEPEND = $(SRCS) $(CPPSRCS) $(CSRCS) $(JAVA_CLASSES:%=%.java)
endif

$(DEPEND): $(ALL_DEPEND) 
	@if [ ! -f "$(DEPEND)" ] ;\
		then \
		{ \
		echo ' ######################################################################' ;\
		echo ' # ' ;\
		echo ' # 	Dependencies created for inclusion in $(DIRNAME)/Makefile.' ;\
		echo " #		`date`" ;\
		echo ' # ' ;\
		echo ' ######################################################################' ;\
		echo '' ;\
		} > $(DEPEND) ;\
	fi
ifneq ($(ALL_DEPEND),)
	@$(ECHO_N) "making dependencies -- "
	@chmod +w $(DEPEND)
	@date=`date` ;\
	for i in  $? ;\
		do \
		$(ECHO_N) "$$i " ;\
		basename=`expr "$$i" : '\(.*\)\..*'` ;\
		ext=`expr "$$i" : '.*\.\(.*\)'` ;\
		{ \
		case "$$ext" in \
		c)	$(MAKE_DEPEND_C) $(DEPEND_FLAGS) "$$i";;\
		cc)	$(MAKE_DEPEND_CXX) $(DEPEND_FLAGS) "$$i";;\
		java)	echo "$$basename.class : $$basename.java";;\
		*)      echo "# Can't make depend for extension $$ext";;\
		esac ;\
		}|\
		$(AWK) -f $(EST)/config/rules/make_depend.awk \
			name="$$i" \
			basename="$$basename" \
			ext="$$ext" \
			date="$$date" \
			$(DEPEND) - > '#xxxx';\
		mv '#xxxx' $(DEPEND);\
	done
	@echo
endif
	@chmod +w $(DEPEND)
	@echo 'INCLUDED_MAKE_DEPEND=1' >> $(DEPEND)
	@chmod -w $(DEPEND)





