CC = gcc
CFLAGS = -O3 -ffast-math -I.. -gdwarf-2 -Wall -Wextra

BUILDDIR = build

OBJS = \
	$(BUILDDIR)/resample.o \

resample: $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $@

# from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/#tldr
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

$(BUILDDIR)/%.o : %.c
$(BUILDDIR)/%.o : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(BUILDDIR)/%.o : %.cc
$(BUILDDIR)/%.o : %.cc $(DEPDIR)/%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(BUILDDIR)/%.o : %.cxx
$(BUILDDIR)/%.o : %.cxx $(DEPDIR)/%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(OBJS))))
#end

.phony: clean

clean:
	rm -f $(DEPDIR)/*.d $(BUILDDIR)/*.o resample.exe
