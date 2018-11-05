#
# local.mk
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

helloflow_CFLAGS := $(fdbclient_CFLAGS)
helloflow_LDFLAGS := $(fdbrpc_LDFLAGS)
helloflow_LIBS := lib/libfdbclient.a lib/libfdbrpc.a lib/libflow.a -ldl $(FDB_TLS_LIB)
helloflow_STATIC_LIBS := $(TLS_LIBS)

helloflow_GENERATED_SOURCES += versions.h

ifeq ($(PLATFORM),linux)
  helloflow_LDFLAGS += -static-libstdc++ -static-libgcc
  helloflow_LIBS += -lpthread -lrt
else ifeq ($(PLATFORM),osx)
  helloflow_LDFLAGS += -lc++
endif

test_helloflow_status: helloflow
	python scripts/test_status.py

bin/helloflow.debug: bin/helloflow
