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
	| '"' error_body '"'
	| '"' ~'"' '"'
;

eval returns [std::string cond] :
	   // bug in arch/powerpc/kernel/Makefile
	   '(' in_eval ')'	{$cond = $in_eval.cond;}
	| '$(' in_eval ')'	{$cond = $in_eval.cond;}
	| '${' in_eval '}'	{$cond = $in_eval.cond;}
	| '$@' | '$%' | '$<' | '$?' | '$^' | '$+' | '$|' | '$*' | '$$'
	| '$' ID
;

in_eval returns [std::string cond] :
	  a1=atom+ (':' atom? '=' word?)?		{$cond = $a1.cond;}
	| 'addprefix' ws+ words (COMMA ws* words?)+
	| 'and' ws+ words (COMMA ws* words?)*
	| 'basename' ws+ words
	| 'call' ws+ words (COMMA (atom_ws_eq | '\\')*)*
	| 'dir' ws+ words
	| 'error' ws+ error_body
	| 'eval' ws+ atom_ws_eq+
	| 'findstring' ws+ atom_ws_eq+ COMMA atom_ws+
	| 'filter' ws+ ~COMMA+ COMMA ws* words
	| 'filter-out' ws+ ~COMMA+ COMMA ws* words
	| 'firstword' ws+ words
	| 'foreach' ws+ atom_ws+ COMMA (':' | atom_ws)+ COMMA atom_ws+
	| 'if' ws+ words COMMA ws* words? (COMMA ws* words?)?
	| 'info' ws+ error_body
	| 'lastword' ws+ words
	| 'notdir' ws+ words
	| 'origin' ws+ words
	| 'or' ws+ words (COMMA words?)*
	| 'patsubst' ws+ f=~COMMA+ COMMA ws* t=words COMMA ws* e=words
	| 'shell' ws+ in_shell+
	| 'sort' ws+ words
	| 'subst' ws+ f=~COMMA+ COMMA ws* t=words? COMMA ws* e=words	{$cond = $e.cond;}
	| 'strip' ws+ words
	| 'warning' ws+ error_body
	| 'wildcard' ws+ ('*' | words)+
	| 'word' ws+ words COMMA ws* words
	| 'words' ws+ words
;

error_body :
	  ('or' | word | ws | '>' | '=' | '\'' | ';' | ':' | ',' | '*')*
	| '(' error_body ')'
;

in_shell :
	  '<' | '>' | '2>' | '&1' | '&2' | '|' | '=' | '\\' | ';' | '?'
	| '$$'
	| ws+ | CONFIG | ID
	| '(' in_shell ')'
	| '$(' in_shell ')'
	| '"' ~'"'* '"'
	| '\'' ~'\''* '\''
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

CONFIG : 'CONFIG_' IDfrag ;
BARE_CORE : 'core-' ('m'|'y') ;
BARE_DRIVERS : 'drivers-' ('m'|'y') ;
BARE_LIBS : 'lib' 's'? '-' ('m'|'y') ;
BARE_NET : 'net-' ('m'|'y') ;
BARE_OBJ : 'obj-' ('m'|'y') ;
BARE_VIRT : 'virt-' ('m'|'y') ;
CSKYABI : 'CSKYABI' ;
BITS : 'BITS' ;
SRCARCH : 'SRCARCH' ;
ID : IDfrag ;
fragment IDfrag : [-+_/.A-Za-z0-9]+ ;
NL : '\r'? '\n' ;
TAB : '\t' ;
COMMA : ',' ;
SPACE : ' ' ;
OTHER : .  ;
