VERSION = $(strip $(shell cat $(topdir)/VERSION.txt))
FULL_VERSION = $(VERSION)

VERSION_CPPFLAGS = -DXCSOAR_VERSION=\"$(VERSION)\"

GIT_COMMIT_ID := $(shell git rev-parse --short --verify HEAD 2>$(NUL))
RELEASE_COMMIT_ID := $(shell git rev-parse --short --verify "v$(VERSION)^{commit}" 2>$(NUL))
VERSION_CPPFLAGS += -DGIT_COMMIT_ID=\"$(GIT_COMMIT_ID)\"
FULL_VERSION := $(FULL_VERSION)~$(GIT_COMMIT_ID)

$(call SRC_TO_OBJ,$(SRC)/Version.cpp): $(topdir)/VERSION.txt
$(call SRC_TO_OBJ,$(SRC)/Version.cpp): CPPFLAGS += $(VERSION_CPPFLAGS)
$(call SRC_TO_OBJ,$(SRC)/Dialogs/dlgCredits.cpp): CPPFLAGS += $(VERSION_CPPFLAGS)
