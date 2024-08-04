name = asciier
builddir = build
bin = $(builddir)/$(name)
releasebin = $(builddir)/$(name)_release

csrc = $(wildcard src/**.c)
dbgflags = -g -lcurses -Wall -Wextra --std=c99 #-fsanitize=address
valgrindflags = --leak-check=full --suppressions=ncurses.supp
relflags = -O2 -lcurses -s
cc = gcc

all: $(bin) release

$(bin): $(csrc)
	$(cc) -o $@ $(dbgflags) $^

run: $(bin)
	rm $(builddir)/** || true
	$(cc) -o $(bin) $(dbgflags) $(csrc)
	./$(bin)

release: $(releasebin)

$(releasebin): $(csrc)
	$(cc) -o $(bin)_release $(relflags) $^

check:
	$(MAKE) release
	DEBUGINFO_URLS="https://debuginfod.archlinux.org" valgrind $(valgrindflags) ./$(bin)_release

clean:
	rm $(builddir)/** || true
