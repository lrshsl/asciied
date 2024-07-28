name = asciier
builddir = build
bin = $(builddir)/$(name)

csrc = $(wildcard src/**.c)
dbgflags = -g -lcurses -Wall -Wextra --std=c99 -fsanitize=address
relflags = -O2 -lcurses -s
cc = gcc

all: $(bin) release

$(bin): $(csrc)
	$(cc) -o $@ $(dbgflags) $^

run: $(bin)
	rm $(builddir)/** || true
	$(cc) -o $(bin) $(dbgflags) $(csrc)
	./$(bin)

release: $(csrc)
	$(cc) -o $(bin)_release $(relflags) $^

clean:
	rm $(builddir)/** || true
