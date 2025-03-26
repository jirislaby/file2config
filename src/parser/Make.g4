grammar Make;

@header {
#include <string>
}

makefile : cmd (NL cmd)* EOF ;

cmd :
	  make_rule					//{print(f"RULE {$start.line}: {$make_rule.text}")}
	|  ('-include' | 'include') ws+ inc=nonNL	//{print(f"INC {$start.line}: {$inc.text}")}
	| ws* 'export' (ws+ (i=ID | i=CONFIG))+		//{print(f"EXPORT {$start.line}: {$i.text}")}
	| ws* e=modified_expr				//{std::cout << $e.text << "\n";}
	| ws*
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
	  words ws* ':' ws* modified_expr
	| words ws* ':' ws* nonNL?
	    (NL rule_cmd?)*
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
	  ws* l=atom_lhs ws*
		op=( ':=' | '+=' | '=' | '?=' ) ws*
		r=atom_rhs? ws*
	# ExprAssign
	| SPACE* ifeq_expr ws* NL
		(cmd NL)*
	  (SPACE* 'else' (ws+ ifeq_expr)? ws* NL
		(cmd NL)*
	  )*
	  SPACE* 'endif' ws*
	# ExprOther
	| SPACE* 'define' ws+ atom ('=' | ws)* NL
		(nonNL NL)*?
	  SPACE* 'endef' ws*
	# ExprOther
	| atom // standalone $(error ...)
	# ExprOther
;

ifeq_expr :
	  ('ifeq' | 'ifneq') ws+ ifeq_cond
	| ('ifdef' | 'ifndef') ws+ (ID | CONFIG)
;

ifeq_cond :
	  '(' atom* COMMA ws* atom* ')'
	| '\'' atom* '\'' ws+ '\'' atom* '\''
	| '"' atom* '"' ws+ '"' atom* '"'
;

atom_lhs returns [std::string cond] :
	  BARE_OBJ		{$cond = $BARE_OBJ.text.back();}
	| (ID
	  | eval		{$cond = $eval.cond;}
	  )+
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

atoms_ws :
	(atom | ws)+
;

atom returns [std::string cond] :
	  CONFIG		{$cond = $CONFIG.text;}
	| BARE_OBJ		{$cond = $BARE_OBJ.text.back();}
	| eval			{$cond = $eval.cond;}
	| ID
	| '%' | '\\#'
	| 'FORCE'
	| '"' atom '"'
	| '"' ~'"' '"'
;

eval returns [std::string cond] :
	   // bug in arch/powerpc/kernel/Makefile
	   '(' in_eval ')'	{$cond = $in_eval.cond;}
	| '$(' in_eval ')'	{$cond = $in_eval.cond;}
	| '${' in_eval '}'	{$cond = $in_eval.cond;}
	| '$@' | '$%' | '$<' | '$?' | '$^' | '$+' | '$|' | '$*'
	| '$' ID
;

in_eval returns [std::string cond] :
	  a1=atom (':' atom? '=' word?)?		{$cond = $a1.cond;}
	| 'addprefix' ws+ atoms_ws (COMMA atoms_ws?)+
	| 'and' ws+ words (COMMA words?)*
	| 'basename' ws+ words
	| 'call' ws+ words (COMMA ('=' | words)+)*
	| 'dir' ws+ words
	| 'error' ws+ ('or' | words | '>' | '=' | '\'')*
	| 'eval' ws+ eval
	| 'findstring' ws+ atoms_ws COMMA atoms_ws
	| 'filter' ws+ ~COMMA+ COMMA atoms_ws
	| 'filter-out' ws+ ~COMMA+ COMMA ws* words
	| 'foreach' ws+ atoms_ws COMMA (':' | atoms_ws)+ COMMA atoms_ws
	| 'if' ws+ words COMMA words? (COMMA words?)?
	| 'notdir' ws+ words
	| 'origin' ws+ words
	| 'or' ws+ words (COMMA words?)*
	| 'patsubst' ws+ f=~COMMA+ COMMA t=words COMMA e=words
	| 'shell' ws+ in_shell+
	| 'sort' ws+ words
	| 'subst' ws+ f=~COMMA+ COMMA t=words? COMMA e=words	{$cond = $e.cond;}
	| 'wildcard' ws+ ('*' | words)+
	| 'word' ws+ words COMMA words
	| 'words' ws+ words
	| BITS
	| SRCARCH
;

in_shell :
	  '<' | '>' | '2>' | '|' | '=' | '\\'
	| ws+ | ID
	| '(' in_shell ')'
	| '$(' in_shell ')'
	| '"' ~'"'* '"'
;

nonNL : ~NL+ ;

ws : SPACE | TAB ;

COMMENT : '#' ~'\n'* -> skip ;
CONT_LINE : '\\' ' '* '\r'? '\n' -> skip ;

CONFIG : 'CONFIG_' IDfrag ;
BARE_OBJ : 'obj-' ('m'|'y') ;
SRCARCH : 'SRCARCH' ;
BITS : 'BITS' ;
ID : IDfrag ;
fragment IDfrag : [-+_/.A-Za-z0-9]+ ;
NL : '\r'? '\n' ;
TAB : '\t' ;
COMMA : ',' ;
SPACE : ' ' ;
OTHER : .  ;
