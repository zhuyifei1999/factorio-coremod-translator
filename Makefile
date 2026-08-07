O := .

INCLUDES := -iquote $(abspath $(O))
CFLAGS := -O2 -pipe -g -Wall \
	-Wfloat-equal -Wcast-align -Waggregate-return -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn \
	-Wmissing-format-attribute -Wunreachable-code -Wimplicit-fallthrough \
	-fno-semantic-interposition -fvisibility=hidden -fPIC
LDFLAGS := $(CFLAGS) -lelf -liberty

PYTHON ?= python
PYTHON_CONFIG ?= $(PYTHON)-config
PYTHON_CFLAGS := $(shell $(PYTHON_CONFIG) --cflags)
PYTHON_LDFLAGS := $(shell $(PYTHON_CONFIG) --ldflags --embed)


SRC_C := $(wildcard *.c)
SRC_S := $(wildcard *.S)
OBJ := $(SRC_C:%.c=$(O)/%.o) $(SRC_S:%.S=$(O)/%.o)

ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s%s\n'			\
		      "$(1)"				\
		      "$(patsubst $(O)/%,%,$(2))"	\
		      "$(if $(3), $(3))";
	MAKEFLAGS += --no-print-directory
	PIPFLAGS += -qq
endif

all: $(O)/libfactoriotranslate.so

-include $(SRC_C:%.c=$(O)/%.d) $(SRC_S:%.S=$(O)/%.d)

.PHONY: clean
.SECONDARY:
.DELETE_ON_ERROR:

clean:
	$(call msg,CLEAN,$(O))
	$(Q)test -d $(O) && find $(O) \( -name '*.o' -o -name '*.d' \) -delete || true
	$(Q)test -d $(O) && cd $(O) && rm -f libfactoriotranslate-native.so || true
	$(Q)test -d $(O) && cd $(O) && rm -f libfactoriotranslate-python.zip || true
	$(Q)test -d $(O) && find $(O) -type d -empty -delete || true

$(O):
	$(Q)mkdir -p $@

$(O)/python.o: python.c | $(O)
	$(Q)mkdir -p $(@D)
	$(call msg,CC,$@)
	$(Q)$(CC) -c $< -o $@ -MD -MP $(PYTHON_CFLAGS) $(CFLAGS) $(INCLUDES)

$(O)/%.o: %.c | $(O)
	$(Q)mkdir -p $(@D)
	$(call msg,CC,$@)
	$(Q)$(CC) -c $< -o $@ -MD -MP $(CFLAGS) $(INCLUDES)

$(O)/%.o: %.S | $(O)
	$(Q)mkdir -p $(@D)
	$(call msg,AS,$@)
	$(Q)$(CC) -c $< -o $@ -MD -MP $(CFLAGS) $(INCLUDES)

$(O)/libfactoriotranslate-native.so: $(OBJ) | $(O)
	$(call msg,LD,$@)
	$(Q)$(CC) -shared $^ -o $@ $(PYTHON_LDFLAGS) $(LDFLAGS)
ifeq ($(DO_STRIP),1)
	$(call msg,STRIP,$@)
	$(Q)$(STRIP) -g $@
endif

$(O)/libfactoriotranslate-python.zip: py py/* py-requirements.txt | $(O)
	$(call msg,BUILD,py-build)
	$(Q)mkdir -p $(O)/py-build && \
	cp -a py/* $(O)/py-build && \
	$(PYTHON) -m pip install --no-binary :all: --no-compile $(PIPFLAGS) -Ur py-requirements.txt --target $(O)/py-build && \
	find $(O)/py-build -name '*.egg-info' -prune -exec rm -r {} \; && \
	find $(O)/py-build -name 'tests' -prune -exec rm -r {} \; && \
	rm -rf $(O)/py-build/*.dist-info || \
	(rm -rf $(O)/py-build; exit 1)
	$(call msg,ZIPAPP,$@)
	$(Q)$(PYTHON) -m zipapp --compress $(O)/py-build -o $@
	$(call msg,RM,py-build)
	$(Q)rm -rf $(O)/py-build


$(O)/libfactoriotranslate.so: $(O)/libfactoriotranslate-native.so $(O)/libfactoriotranslate-python.zip | $(O)
	$(call msg,CAT,$@)
	$(Q)cat $^ > $@ && chmod a+x $@
