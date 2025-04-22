// SPDX-License-Identifier: GPL-2.0-only

grammar Make;

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
	i=('-include' | 'include') ws+ inc=nonNL	//{std::cout << "INC " << $i.line << ": " << $inc.text << '\n';}
;

export :
	('export' | 'unexport') (ws+ (ID | CONFIG | BITS | SRCARCH))+
;

modifier :
	  'export'
	| 'override'
	| 'private'
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
	words ws* (':' | '&:')
;

rule_cmd :
	  TAB nonNL?
	| SPACE* ifeq_expr ws* NL
		(rule_cmd? NL)*
	  (SPACE* 'else' (ws+ ifeq_expr)? ws* NL
		(rule_cmd? NL)*
	  )*
	  SPACE* 'endif' ws*
;

expr :
	  l=atom_lhs ws*
		op=( ':=' | '+=' | '=' | '?=' ) ws*
		(r=atom_rhs ws*)?
;

conditional_or_macro :
	  conditional_or_macro_ws ifeq_expr ws* NL
		(cmd NL)*
	  (conditional_or_macro_ws 'else' (ws+ ifeq_expr)? ws* NL
		(cmd NL)*
	  )*
	  conditional_or_macro_ws 'endif' ws*
	| conditional_or_macro_ws 'define' ws+ atom ('=' | ws)* NL
		(nonNL? NL)*?
	  conditional_or_macro_ws 'endef' ws*
	| conditional_or_macro_ws 'undefine' ws+ atom
;

conditional_or_macro_ws:
	// TAB for pre-3b9ab248bc45 -- make does not allow TABs in fact...
	TAB* SPACE*
;

ifeq_expr :
	  ('ifeq' | 'ifneq') ws+ ifeq_cond
	| ('ifdef' | 'ifndef') ws+ (ID | CONFIG)
;

ifeq_cond :
	  '(' ifeq_atom* COMMA ws* ifeq_atom* ')'
	| '\'' atom* '\'' ws+ '\'' atom* '\''
	| '"' atom* '"' ws+ '"' atom* '"'
;

ifeq_atom :
	  atom | '*' | ':' | '='
;

atom_lhs returns [std::string cond] :
	  bare			{$cond = $bare.cond;}
	| ( id
	  | eval		{$cond = $eval.cond;}
	  )+
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
	atom_ws | '='
;

atom_ws :
	atom | ws
;

atom returns [std::string cond] :
	  id			{$cond = $id.cond;}
	| bare			{$cond = $bare.cond;}
	| eval			{$cond = $eval.cond;}
	| '%' | '\\#'
	| '!' // 3.0 and drivers/lguest/Makefile
	| 'FORCE'
	| '"' ~'"'*? '"'
;

eval returns [std::string cond] :
	  function		{$cond = $function.cond;}
	   // bug in arch/powerpc/kernel/Makefile
	|  '(' in_eval ')'	{$cond = $in_eval.cond;}
	| '$(' in_eval ')'	{$cond = $in_eval.cond;}
	| '${' in_eval '}'	{$cond = $in_eval.cond;}
	| '$@' | '$%' | '$<' | '$?' | '$^' | '$+' | '$|' | '$*' | '$$'
	| '$' ID
;

function returns [std::string cond] :
	  FUN_ADDPREFIX ws* words (COMMA ws* words?)+ ')'
	| FUN_AND ws* words (COMMA ws* words?)* ')'
	| FUN_BASENAME ws* words ')'
	| FUN_CALL ws* words (COMMA (atom_ws_eq | '\\')*)* ')'
	| FUN_DIR ws* words ')'
	| FUN_EVAL ws* atom_ws_eq+ ')'
	| FUN_FINDSTRING ws* atom_ws_eq+ COMMA atom_ws+ ')'
	| FUN_FILTER ws* ~COMMA+ COMMA ws* words ')'
	| FUN_FILTER_OUT ws* ~COMMA+ COMMA ws* words ')'
	| FUN_FIRSTWORD ws* words ')'
	| FUN_FOREACH ws* atom_ws+ COMMA (':' | atom_ws)+ COMMA atom_ws+ ')'
	| FUN_IF ws* words COMMA ws* words? (COMMA ws* words?)? ')'
	| FUN_LASTWORD ws* words ')'
	| FUN_NOTDIR ws* words ')'
	| FUN_ORIGIN ws* words ')'
	| FUN_OR ws* words (COMMA words?)* ')'
	| FUN_PATSUBST ws* f=~COMMA+ COMMA ws* t=words COMMA ws* e=words ')'
	| FUN_SORT ws* words ')'
	| FUN_SUBST ws* f=~COMMA+ COMMA ws* t=words? COMMA ws* e=words ')'	{$cond = $e.cond;}
	| FUN_STRIP ws* words ')'
	| FUN_WILDCARD ws* ('*' | words)+ ')'
	| FUN_WORD ws* words COMMA ws* words ')'
	| FUN_WORDS ws* words ')'
;

in_eval returns [std::string cond] :
	  a1=atom+ (':' atom? '=' word?)?		{$cond = $a1.cond;}
;

nonNL : ~NL+ ;

ws : SPACE | TAB ;

MULTILINE_COMMENT :
	'#' COMMENT_BODY (FRAG_CONT_LINE COMMENT_BODY)*
	-> skip
;
fragment COMMENT_BODY : ~('\n' | '\\')* ;
CONT_LINE : FRAG_CONT_LINE -> skip;
fragment FRAG_CONT_LINE : '\\' ' '* '\r'? '\n';

ERROR : '$(' ('error'|'warning'|'info') WS SKIP_BODY ')' -> skip ;
SHELL : '$(shell' WS SKIP_BODY ')' -> skip ;
fragment SKIP_BODY :
	  ( ~[()] | '(' SKIP_BODY ')' )*
;

FUN_ADDPREFIX :		'$(addprefix' WS ;
FUN_AND :		'$(and' WS ;
FUN_BASENAME :		'$(basename' WS ;
FUN_CALL :		'$(call' WS ;
FUN_DIR : 		'$(dir' WS ;
FUN_EVAL :		'$(eval' WS ;
FUN_FINDSTRING :	'$(findstring' WS ;
FUN_FILTER :		'$(filter' WS ;
FUN_FILTER_OUT :	'$(filter-out' WS ;
FUN_FIRSTWORD :		'$(firstword' WS ;
FUN_FOREACH :		'$(foreach' WS ;
FUN_IF :		'$(if' WS ;
FUN_LASTWORD :		'$(lastword' WS ;
FUN_NOTDIR :		'$(notdir' WS ;
FUN_OR : 		'$(or' WS ;
FUN_ORIGIN :		'$(origin' WS ;
FUN_PATSUBST :		'$(patsubst' WS ;
FUN_SORT :		'$(sort' WS ;
FUN_SUBST :		'$(subst' WS ;
FUN_STRIP :		'$(strip' WS ;
FUN_WILDCARD :		'$(wildcard' WS ;
FUN_WORD :		'$(word' WS ;
FUN_WORDS :		'$(words' WS ;

CONFIG : 'CONFIG_' ID ;
BARE_CORE : 'core-' ('m'|'y') ;
BARE_DRIVERS : 'drivers-' ('m'|'y') ;
BARE_LIBS : 'lib' 's'? '-' ('m'|'y') ;
BARE_NET : 'net-' ('m'|'y') ;
BARE_OBJ : 'obj-' ('m'|'y') ;
BARE_VIRT : 'virt-' ('m'|'y') ;
CSKYABI : 'CSKYABI' ;
BITS : 'BITS' ;
SRCARCH : 'SRCARCH' ;
ID : [-+_/.A-Za-z0-9]+ ;
NL : '\r'? '\n' ;
fragment WS : SPACE | TAB ;
TAB : '\t' ;
COMMA : ',' ;
SPACE : ' ' ;
OTHER : .  ;
