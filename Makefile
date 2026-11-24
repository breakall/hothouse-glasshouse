TARGET = glasshouse
APP_TYPE = BOOT_SRAM
LIBDAISY_DIR = vendor/HothouseExamples/libDaisy
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
CPP_SOURCES = firmware/main.cpp vendor/HothouseExamples/src/hothouse.cpp
C_INCLUDES = -Ifirmware -Ivendor/HothouseExamples/src
OPT = -O2
CPP_STANDARD = -std=gnu++14
ifneq ($(MAKECMDGOALS),test)
include $(SYSTEM_FILES_DIR)/Makefile
endif

.PHONY: test dependencies release diagnostic
dependencies:
	$(MAKE) -C $(LIBDAISY_DIR) -j4
test:
	@mkdir -p build
	c++ -std=c++14 -O2 -Wall -Wextra -Werror -Ifirmware tests/test.cpp -o build/test
	./build/test
release: all
	@mkdir -p release
	cp build/glasshouse.bin release/glasshouse-seed.bin
	shasum -a 256 release/glasshouse-seed.bin > release/SHA256SUMS
diagnostic:
	$(MAKE) all TARGET=glasshouse-diagnostic BUILD_DIR=build/diagnostic OPT="-O2 -DGLASSHOUSE_DIAGNOSTICS"
	cp build/diagnostic/glasshouse-diagnostic.bin release/glasshouse-diagnostic.bin
