name = asciied

# Where to (find | put) stuff
docdir = docs/generated
srcdir = src
testdir = tests
builddir = build

# Output
debugbin = $(builddir)/$(name)
releasebin = $(builddir)/$(name)_release
testbin = $(builddir)/$(name)_tests

# How to build
cc = gcc
cflags = -Iinclude -lcurses -funsigned-char -funsigned-bitfields
dbgflags = -g -ggdb -Wall -Wextra --std=c99 #-fsanitize=address
testflags = $(dbgflags) -D IS_TEST_BUILD=1
valgrindflags = --leak-check=full --suppressions=ncurses.supp
relflags = -O3 -s

# Don't touch
csrc = $(wildcard $(srcdir)/**.c)
tsrc = $(csrc) \
		 $(wildcard $(testdir)/**.c)


###############################################################################
#                                    Rules                                    #
###############################################################################

.PHONY: all debug release test run configure check docs clean cleanall

# Default: build debug and release
all: debug release

# Only debug, release or test
debug: $(debugbin)
release: $(releasebin)

test: $(testbin)
	./$^

# Build and run debug executable
run: $(debugbin)
	./$^

# How to build the executables
$(debugbin): $(csrc)
	@mkdir -p $(builddir)
	$(cc) -o $@ $(cflags) $(dbgflags) $^

$(releasebin): $(csrc)
	@mkdir -p $(builddir)
	$(cc) -o $@ $(cflags) $(relflags) $^

$(testbin): $(tsrc)
	@mkdir -p $(builddir)
	$(cc) -o $@ $(cflags) $(testflags) $^

# Check with valgrind for memory leaks
# > Note: ncurses.supp is used in order to suppress some
# > intentional leaks in ncurses
check:
	@$(MAKE) release
	DEBUGINFO_URLS="https://debuginfod.archlinux.org" \
						valgrind $(valgrindflags) ./$(debugbin)_release

configure:
	bear -- $(MAKE) debug
	bear --append -- $(MAKE) release
	bear --append -- $(MAKE) test

# Generate documentation
# > Note: The output directory should correspond to $(docdir)
docs:
	doxygen doxyfile

# Since cleanup removes the $(builddir)
$(builddir):
	@if [ ! -d $(builddir) ]; then mkdir $(builddir); fi

# Clean up
clean:
	rm -fr $(builddir)/ || true

# Clean everything
cleanall:
	rm -rf            \
		$(builddir)/   \
		log/**         \
		$(docdir)      \
		compile_commands.json \
	|| true

