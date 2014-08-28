
JSHINT = node_modules/.bin/jshint
MOCHA = node_modules/.bin/mocha

sources = hmsearch/hmsearch.h hmsearch/hmsearch.cc ext/hmnode.cc
target = build/Release/hmsearch.node
gen-config = build/Makefile
binding = binding.gyp

jshint-files = index.js tests/test_*.js
mocha-files = tests/test_*.js

REPORTER=
ifeq ($(EMACS),t)
REPORTER=--reporter=.jshint-emacs.js
endif

all: $(target) lint

$(target): $(gen-config) $(sources)
	node-gyp build

$(gen-config): $(binding)
	node-gyp configure

lint:
	$(JSHINT) $(REPORTER) $(jshint-files)

clean:
	node-gyp clean

test: 
	$(MOCHA) $(MOCHA_FLAGS) $(mocha-files)

.PHONY: all lint clean test
