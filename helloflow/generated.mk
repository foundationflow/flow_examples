#
# vcxproj.mk
#
# This source file is part of the FoundationDB open source project
#
# Copyright 2013-2018 Apple Inc. and the FoundationDB project authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# -*- mode: makefile; -*-

TARGETS += helloflow
CLEAN_TARGETS += helloflow_clean

helloflow_ALL_SOURCES := $(addprefix helloflow/,FlowLineNoise.actor.cpp FlowLineNoise.h helloflow.actor.cpp linenoise/linenoise.c linenoise/linenoise.h)

helloflow_BUILD_SOURCES := $(patsubst %.actor.cpp,${OBJDIR}/%.actor.g.cpp,$(filter-out %.h %.hpp,$(helloflow_ALL_SOURCES)))
helloflow_GENERATED_SOURCES := $(patsubst %.actor.h,%.actor.g.h,$(patsubst %.actor.cpp,${OBJDIR}/%.actor.g.cpp,$(filter %.actor.h %.actor.cpp,$(helloflow_ALL_SOURCES))))
GENERATED_SOURCES += $(helloflow_GENERATED_SOURCES)

-include helloflow/local.mk

# We need to include the current directory for .g.actor.cpp files emitted into
# .objs that use includes not based at the root of fdb.
helloflow_CFLAGS := -I helloflow -I ${OBJDIR}/helloflow ${helloflow_CFLAGS}

# If we have any static libs, we have to wrap them in the appropriate
# compiler flag magic
ifeq ($(helloflow_STATIC_LIBS),)
  helloflow_STATIC_LIBS_REAL :=
else
# MacOS doesn't recognize -Wl,-Bstatic, but is happy with -Bstatic
# gcc will handle both, so we prefer the non -Wl version
  helloflow_STATIC_LIBS_REAL := -Bstatic $(helloflow_STATIC_LIBS) -Bdynamic
endif

# If we have any -L directives in our LDFLAGS, we need to add those
# paths to the VPATH
VPATH += $(addprefix :,$(patsubst -L%,%,$(filter -L%,$(helloflow_LDFLAGS))))

IGNORE := $(shell echo $(VPATH))

helloflow_OBJECTS := $(addprefix $(OBJDIR)/,$(filter-out $(OBJDIR)/%,$(helloflow_BUILD_SOURCES:=.o))) $(filter $(OBJDIR)/%,$(helloflow_BUILD_SOURCES:=.o))
helloflow_DEPS := $(addprefix $(DEPSDIR)/,$(helloflow_BUILD_SOURCES:=.d))

.PHONY: helloflow_clean helloflow

helloflow: bin/helloflow

$(CMDDIR)/helloflow/compile_commands.json: build/project_commands.py ${helloflow_ALL_SOURCES}
	@mkdir -p $(basename $@)
	@build/project_commands.py --cflags="$(CFLAGS) $(helloflow_CFLAGS)" --cxxflags="$(CXXFLAGS) $(helloflow_CXXFLAGS)" --sources="$(helloflow_ALL_SOURCES)" --out="$@"

-include $(helloflow_DEPS)

$(OBJDIR)/helloflow/%.actor.g.cpp: helloflow/%.actor.cpp $(ACTORCOMPILER)
	@echo "Actorcompiling $<"
	@mkdir -p $(OBJDIR)/$(<D)
	@$(MONO) $(ACTORCOMPILER) $< $@ >/dev/null

helloflow/%.actor.g.h: helloflow/%.actor.h $(ACTORCOMPILER)
	@if [ -e $< ]; then echo "Actorcompiling $<" ; $(MONO) $(ACTORCOMPILER) $< $@ >/dev/null ; fi
.PRECIOUS: $(OBJDIR)/helloflow/%.actor.g.cpp helloflow/%.actor.g.h

# The order-only dependency on the generated .h files is to force make
# to actor compile all headers before attempting compilation of any .c
# or .cpp files. We have no mechanism to detect dependencies on
# generated headers before compilation.

$(OBJDIR)/helloflow/%.cpp.o: helloflow/%.cpp $(ALL_MAKEFILES) | $(filter %.h,$(GENERATED_SOURCES))
	@echo "Compiling      $(<:${OBJDIR}/%=%)"
ifeq ($(VERBOSE),1)
	@echo $(CCACHE_CXX) $(CFLAGS) $(CXXFLAGS) $(helloflow_CFLAGS) $(helloflow_CXXFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@
endif
	@mkdir -p $(DEPSDIR)/$(<D) && \
	mkdir -p $(OBJDIR)/$(<D) && \
	$(CCACHE_CXX) $(CFLAGS) $(CXXFLAGS) $(helloflow_CFLAGS) $(helloflow_CXXFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@ && \
	cp $(DEPSDIR)/$<.d.tmp $(DEPSDIR)/$<.d && \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(DEPSDIR)/$<.d.tmp >> $(DEPSDIR)/$<.d && \
	rm $(DEPSDIR)/$<.d.tmp

$(OBJDIR)/helloflow/%.cpp.o: $(OBJDIR)/helloflow/%.cpp $(ALL_MAKEFILES) | $(filter %.h,$(GENERATED_SOURCES))
	@echo "Compiling      $(<:${OBJDIR}/%=%)"
ifeq ($(VERBOSE),1)
	@echo $(CCACHE_CXX) $(CFLAGS) $(CXXFLAGS) $(helloflow_CFLAGS) $(helloflow_CXXFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@
endif
	@mkdir -p $(DEPSDIR)/$(<D) && \
	mkdir -p $(OBJDIR)/$(<D) && \
	$(CCACHE_CXX) $(CFLAGS) $(CXXFLAGS) $(helloflow_CFLAGS) $(helloflow_CXXFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@ && \
	cp $(DEPSDIR)/$<.d.tmp $(DEPSDIR)/$<.d && \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(DEPSDIR)/$<.d.tmp >> $(DEPSDIR)/$<.d && \
	rm $(DEPSDIR)/$<.d.tmp

$(OBJDIR)/helloflow/%.c.o: helloflow/%.c $(ALL_MAKEFILES) | $(filter %.h,$(GENERATED_SOURCES))
	@echo "Compiling      $<"
ifeq ($(VERBOSE),1)
	@echo "$(CCACHE_CC) $(CFLAGS) $(helloflow_CFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@"
endif
	@mkdir -p $(DEPSDIR)/$(<D) && \
	mkdir -p $(OBJDIR)/$(<D) && \
	$(CCACHE_CC) $(CFLAGS) $(helloflow_CFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@ && \
	cp $(DEPSDIR)/$<.d.tmp $(DEPSDIR)/$<.d && \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(DEPSDIR)/$<.d.tmp >> $(DEPSDIR)/$<.d && \
	rm $(DEPSDIR)/$<.d.tmp

$(OBJDIR)/helloflow/%.S.o: helloflow/%.S $(ALL_MAKEFILES) | $(filter %.h,$(GENERATED_SOURCES))
	@echo "Assembling     $<"
ifeq ($(VERBOSE),1)
	@echo "$(CCACHE_CC) $(CFLAGS) $(helloflow_CFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@"
endif
	@mkdir -p $(DEPSDIR)/$(<D) && \
	mkdir -p $(OBJDIR)/$(<D) && \
	$(CCACHE_CC) $(CFLAGS) $(helloflow_CFLAGS) -MMD -MT $@ -MF $(DEPSDIR)/$<.d.tmp -c $< -o $@ && \
	cp $(DEPSDIR)/$<.d.tmp $(DEPSDIR)/$<.d && \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(DEPSDIR)/$<.d.tmp >> $(DEPSDIR)/$<.d && \
	rm $(DEPSDIR)/$<.d.tmp

helloflow_clean:
	@echo "Cleaning       helloflow"
	@rm -f bin/helloflow $(helloflow_GENERATED_SOURCES) bin/helloflow.debug bin/helloflow-debug
	@rm -rf $(DEPSDIR)/helloflow
	@rm -rf $(OBJDIR)/helloflow

bin/helloflow: $(helloflow_OBJECTS) $(helloflow_LIBS) $(ALL_MAKEFILES) build/link-wrapper.sh build/link-validate.sh
	@mkdir -p bin
	@./build/link-wrapper.sh Application helloflow $@ $(TARGET_LIBC_VERSION)
