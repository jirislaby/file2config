# SPDX-License-Identifier: GPL-2.0-only

GRAMMAR := Make
GENERATED := $(GRAMMAR)Lexer.java $(GRAMMAR)Parser.java
CLASSES := $(patsubst %.java,%.class,$(GENERATED))
CLASSPATH := $(shell build-classpath antlr4-runtime)
JAVAC := javac
ANTLR4 := antlr4

all: $(CLASSES)

%.class: %.java
	$(JAVAC) -cp $(CLASSPATH) $<

$(GENERATED) &: $(GRAMMAR).g4
	$(ANTLR4) -no-listener -message-format gnu $<

$(GRAMMAR).g4: ../$(GRAMMAR).g4
	sed 's@^#include.*@@; s@{$$cond[^}]*} *@@g; s@returns \[std::string cond\] *@@' $< > $@

clean:
	rm -f *.class *.java *.tokens *.g4 *.interp

.PHONY: clean
