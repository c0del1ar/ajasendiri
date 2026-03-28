CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pedantic -Iinclude -MMD -MP
THREAD_FLAGS ?= -pthread
OPENSSL_CFLAGS ?= $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS ?= $(shell pkg-config --libs openssl 2>/dev/null)

.PHONY: all run test lsp fmt fmt-check mmk-init mmk-install docs docs-check docs-linkcheck docs-clean clean

SRC := src/cli/main.c src/lexer/lexer.c src/parser/parser.c src/runtime/runtime.c
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)
AJA_FMT_PATHS := examples libs tests/spec/pass
DOCS_PYTHON := $(if $(wildcard pyvenv/bin/python),pyvenv/bin/python,python3)

all: ajasendiri

ajasendiri: $(OBJ)
	$(CC) $(CFLAGS) $(OPENSSL_CFLAGS) $(THREAD_FLAGS) $(OBJ) $(OPENSSL_LIBS) -o ajasendiri

run: ajasendiri
	./ajasendiri examples/ok.aja

test: ajasendiri
	./ajasendiri test
	./tests/test_mmk.sh
	./tests/test_venv.sh
	./tests/test_repl.sh
	./tests/test_debug.sh
	./tests/test_fmt.sh
	./tests/test_lsp.sh

lsp:
	python3 tools/ajasendiri_lsp.py

fmt: ajasendiri
	./ajasendiri fmt $(AJA_FMT_PATHS)

fmt-check: ajasendiri
	./ajasendiri fmt --check $(AJA_FMT_PATHS)

mmk-init: ajasendiri
	./ajasendiri mmk init

mmk-install: ajasendiri
	./ajasendiri mmk install

docs:
	python3 -m sphinx -b html docs docs/_build/html

docs-check:
	python3 -m sphinx -W -b html docs docs/_build/html

docs-linkcheck:
	python3 -m sphinx -b linkcheck docs docs/_build/linkcheck

docs-clean:
	rm -rf docs/_build

clean:
	rm -f ajasendiri
	find src -type f \( -name '*.o' -o -name '*.d' \) -delete

-include $(DEP)
