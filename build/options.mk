LLVM ?= n
THIN_LTO ?= n
CLANG ?= $(THIN_LTO)
LTO ?= $(THIN_LTO)
IWYU ?= n
FUZZER ?= n

# Build the fuzzers with libFuzzer? (i.e. -fsanitize=fuzzer) - (for
# Honggfuzz, this should be disabled)
LIBFUZZER ?= n

# Prefer packages from Homebrew?
USE_HOMEBREW ?= n

# shall we paint with some eye candy?
EYE_CANDY ?= $(call bool_not,$(TARGET_IS_KOBO))
ifeq ($(EYE_CANDY),y)
  TARGET_CPPFLAGS += -DEYE_CANDY
  WINDRESFLAGS += -DEYE_CANDY
endif

# Enable gcc/clang sanitizers?  Either "n" to disable, "y" to enable
# default sanitizers or a comma-separated list of sanitizers
# (e.g. "address,undefined").
SANITIZE ?= n

# Identical code folding.  This must stay disabled for sanitizer builds:
# AddressSanitizer gives each global a one-byte "ODR indicator" object of
# its own, and the linker happily folds all of those identical objects
# into one, after which the second global to register itself at startup
# is reported as an odr-violation of the first.
ifeq ($(DEBUG)$(HAVE_WIN32)$(TARGET_IS_DARWIN)$(SANITIZE),nnnn)
  ICF ?= y
else
  ICF ?= n
endif

# show map renderer times?
STOP_WATCH ?= n
ifeq ($(STOP_WATCH),y)
  TARGET_CPPFLAGS += -DSTOP_WATCH
endif

# compile without UI?
HEADLESS ?= n

ifeq ($(TARGET_IS_KOBO),y)
  DITHER ?= y
else
  DITHER ?= n
endif

ifeq ($(DITHER),y)
  TARGET_CPPFLAGS += -DDITHER
endif

GREYSCALE ?= $(DITHER)

ifeq ($(GREYSCALE),y)
  TARGET_CPPFLAGS += -DGREYSCALE
endif

# When enabled, the Androidpackage org.xcsoar.testing is created, with
# a red Activity icon, to allow simultaneous installation of "stable"
# and "testing".
# In the stable branch, this should default to "n".
TESTING = y

ifeq ($(TESTING),y)
  TARGET_CPPFLAGS += -DXCSOAR_TESTING
endif
