// SPDX-License-Identifier: GPL-2.0-only

parser grammar MakeParser;

options { tokenVocab=MakeLexer; }

@header {
#include <string>
}

makefile : cmd (NL cmd)* EOF ;

cmd :
	  ws* make_rule					//{print(f"RULE {$start.line}: {$make_rule.text}")}
	| ws* include
	| ws* export
	| ws* e=modified_expr				//{std::cout << $e.text << "\n";}
	| conditional_or_macro
	| ws* atom // standalone $(error ...)
	| ws*
;

include :
	INCLUDE ws+ word
;

export :
	(EXPORT | UNEXPORT) (ws+ (ID | CONFIG | BITS | SRCARCH))+
;

modifier :
	  EXPORT
	| OVERRIDE
	| PRIVATE
;

modified_expr :
	(modifier ws+)? expr
;

make_rule :
	  make_rule_lhs ws* modified_expr
	| make_rule_lhs ws* nonNL?
	    (NL rule_cmd?)*
;

make_rule_lhs:
	words ws* (COLON | RULE_SEP)
;

rule_cmd :
	  TAB nonNL?
	| SPACE* ifeq_expr ws* NL
		(rule_cmd? NL)*
	  (SPACE* ELSE (ws+ ifeq_expr)? ws* NL
		(rule_cmd? NL)*
	  )*
	  SPACE* ENDIF ws*
;

expr :
	  l=atom_lhs ws*
		op=( EQ | ASSIGN ) ws*
		(r=atom_rhs ws*)?
;

conditional_or_macro :
	  conditional_or_macro_ws ifeq_expr ws* NL
		(cmd NL)*
	  (conditional_or_macro_ws ELSE (ws+ ifeq_expr)? ws* NL
		(cmd NL)*
	  )*
	  conditional_or_macro_ws ENDIF ws*
	| conditional_or_macro_ws DEFINE ws+ atom (EQ | ws)* NL
		(nonNL? NL)*?
	  conditional_or_macro_ws ENDEF ws*
	| conditional_or_macro_ws UNDEFINE ws+ atom
;

conditional_or_macro_ws:
	// TAB for pre-3b9ab248bc45 -- make does not allow TABs in fact...
	TAB* SPACE*
;

ifeq_expr :
	  (IFEQ | IFNEQ) ws+ ifeq_cond
	| (IFDEF | IFNDEF) ws+ (ID | CONFIG)
;

ifeq_cond :
	  LPAREN ifeq_atom* COMMA ws* ifeq_atom* RPAREN
	| STRING ws+ STRING
;

ifeq_atom :
	  atom | ASTERISK | COLON | EQ
;

atom_lhs returns [std::string cond] :
	  bare			{$cond = $bare.cond;}
	| ( id
	  | eval		{$cond = $eval.cond;}
	  )+
	// arch/m68k/kernel/Makefile_mm: obj-y$(CONFIG_MMU_SUN3)
	| bare eval		{$cond = $eval.cond;}
;

id returns [std::string cond] :
	  BITS
	| CONFIG	{$cond = $CONFIG.text;}
	| CSKYABI
	| ID
	| SRCARCH
;

bare returns [std::string cond] :
	  BARE_CORE		{$cond = $BARE_CORE.text.back();}
	| BARE_DRIVERS		{$cond = $BARE_DRIVERS.text.back();}
	| BARE_LIBS		{$cond = $BARE_LIBS.text.back();}
	| BARE_NET		{$cond = $BARE_NET.text.back();}
	| BARE_OBJ		{$cond = $BARE_OBJ.text.back();}
	| BARE_VIRT		{$cond = $BARE_VIRT.text.back();}
;

atom_rhs :
	  words						// try to eat it as words, but:
	| un=nonNL //{std::cout << "UNHANDLED " << $un.text << "\n";}	// slurp anything else
;

words returns [std::string cond] :
	w+=word {$cond = $word.cond;} (ws+ w+=word {$cond = $word.cond;})*
;

word returns [std::string cond] :
	(atom {$cond = $atom.cond;})+
;

atom_ws_eq :
	atom_ws | EQ
;

atom_ws :
	atom | ws
;

atom returns [std::string cond] :
	  id			{$cond = $id.cond;}
	| bare			{$cond = $bare.cond;}
	| eval			{$cond = $eval.cond;}
	| PERCENT | HASH
	| EXCLAMATION // 3.0 and drivers/lguest/Makefile
	| FORCE
	| STRING
;

eval returns [std::string cond] :
	  function		{$cond = $function.cond;}
	   // bug in arch/powerpc/kernel/Makefile
	|  LPAREN in_eval RPAREN	{$cond = $in_eval.cond;}
	| EVALPAR in_eval RPAREN	{$cond = $in_eval.cond;}
	| EVALBRK in_eval RBRK		{$cond = $in_eval.cond;}
	| DOLL_BUILTIN
	| DOLL_ID
;

function returns [std::string cond] :
	  FUN_ABSPATH ws* words RPAREN
	| FUN_ADDPREFIX ws* words (COMMA ws* words?)+ RPAREN
	| FUN_ADDSUFFIX ws* words (COMMA ws* words?)+ RPAREN
	| FUN_AND ws* words (COMMA ws* words?)* RPAREN
	| FUN_BASENAME ws* words RPAREN
	| FUN_CALL ws* words (COMMA (atom_ws_eq | ESC)*)* RPAREN
	| FUN_DIR ws* words RPAREN
	| FUN_EVAL ws* atom_ws_eq+ RPAREN
	| FUN_FILE ws* (LT | GT | SHR) ws+ word (COMMA ws* words)? RPAREN
	| FUN_FILTER_OUT ws* ~COMMA+ COMMA ws* words RPAREN
	| FUN_FILTER ws* ~COMMA+ COMMA ws* words RPAREN
	| FUN_FINDSTRING ws* atom_ws_eq+ COMMA atom_ws+ RPAREN
	| FUN_FIRSTWORD ws* words RPAREN
	| FUN_FLAVOR ws* word RPAREN
	| FUN_FOREACH ws* atom_ws+ COMMA (COLON | atom_ws)+ COMMA atom_ws+ RPAREN
	| FUN_IF ws* words COMMA ws* words? (COMMA ws* words?)? RPAREN
	| FUN_INTCMP ws* words (COMMA ws* words?)+ RPAREN
	| FUN_JOIN ws* words COMMA ws* words RPAREN
	| FUN_LASTWORD ws* words RPAREN
	| FUN_LET ws* words COMMA ws* words? COMMA ws* words? RPAREN
	| FUN_NOTDIR ws* words RPAREN
	| FUN_ORIGIN ws* words RPAREN
	| FUN_OR ws* words (COMMA words?)* RPAREN
	| FUN_PATSUBST ws* f=~COMMA+ COMMA ws* t=words COMMA ws* e=words RPAREN
	| FUN_REALPATH ws* words RPAREN
	| FUN_SORT ws* words RPAREN
	| FUN_STRIP ws* words ws* RPAREN
	| FUN_SUBST ws* f=~COMMA+ COMMA ws* t=words? COMMA ws* e=words RPAREN	{$cond = $e.cond;}
	| FUN_SUFFIX ws* words RPAREN
	| FUN_VALUE ws* word RPAREN
	| FUN_WILDCARD ws* (ASTERISK | words)+ RPAREN
	| FUN_WORDLIST ws* word COMMA ws* word COMMA ws* words RPAREN
	| FUN_WORDS ws* words RPAREN
	| FUN_WORD ws* words COMMA ws* words RPAREN
;

in_eval returns [std::string cond] :
	  a1=atom+ (COLON atom? EQ word?)?		{$cond = $a1.cond;}
;

nonNL : ~NL+ ;

ws : SPACE | TAB ;
