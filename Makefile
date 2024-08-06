name = asciied

# Where to (find | put) stuff
docdir = docs/generated
srcdir = src
builddir = build

# Output
debugbin = $(builddir)/$(name)
releasebin = $(builddir)/$(name)_release

# How to build
cc = gcc
dbgflags = -g -lcurses -Wall -Wextra --std=c99 #-fsanitize=address
valgrindflags = --leak-check=full --suppressions=ncurses.supp
relflags = -O2 -lcurses -s

# Don't touch
csrc = $(wildcard $(srcdir)/**.c)


###############################################################################
#                                    Rules                                    #
###############################################################################

.PHONY: all debug release run check docs clean cleanall

# Default: build debug and release
all: debug release

# Only debug or release
debug: $(debugbin)
release: $(releasebin)

# Build and run debug executable
run: $(debugbin)
	./$^

# How to build the executables
$(debugbin): $(csrc)
	@mkdir -p $(builddir)
	$(cc) -o $@ $(dbgflags) $^

$(releasebin): $(csrc)
	@mkdir -p $(builddir)
	$(cc) -o $@ $(relflags) $^

# Check with valgrind for memory leaks
# > Note: ncurses.supp is used in order to suppress some
# > intentional leaks in ncurses
check:
	@$(MAKE) release
	DEBUGINFO_URLS="https://debuginfod.archlinux.org" \
						valgrind $(valgrindflags) ./$(debugbin)_release

# Generate documentation
# > Note: The output directory should correspond to $(docdir)
docs:
	doxygen Doxyfile

# Since cleanup removes the $(builddir)
$(builddir):
	@if [ ! -d $(builddir) ]; then mkdir $(builddir); fi

# Clean up
clean:
	rm -fr $(builddir)/ || true

# Clean everything
cleanall:
	rm -fr $(builddir)/ || true
	rm -fr log/** || true
	rm -fr $(docdir) || true
