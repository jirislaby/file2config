grammar Make;

@header {
#include <string>
}

makefile : cmd* EOF ;

cmd :
	  makeRule				//XX{print(f"RULE {$start.line}: {$makeRule.info}")}
	| ('-include' | 'include') ws+ inc=nonNL NL	//{print(f"INC {$start.line}: {$inc.text}")}
	| ws* 'export' (ws+ (i=ID | i=CONFIG))+ NL	//{print(f"EXPORT {$start.line}: {$i.text}")}
	| ws* e=modified_expr NL		//{std::cout << $e.text << "\n";}
			  /*{e = $e.text
e = e.split("\n", 2)[0][:100]
print(f"EXP {$start.line}: {e} --- ({$e.info})")
}*/
	| NL
;

modifier :
	  'export'
	| 'override'
	| 'private'
;

modified_expr returns [std::string info] :
	(modifier ws+)? expr				{$info=$expr.info;}
;

makeRule returns [std::string info] :
	  atoms {$info=$atoms.text;} ':' ws* e=modified_expr NL		/*{e = $e.text
e = e.split("\n", 2)[0][:100]
print(f"RULE-EXP {$start.line}: {e} --- ({$e.info})")
}*/
	| atoms {$info=$atoms.text;} ':' (r=nonNL /*{$info += ' RRR ' + $r.text;}*/ )? NL
	    rule_cmd*
;

rule_cmd:
	  (TAB nonNL?)? NL
	| SPACE* ifeq_expr ws* NL
		rule_cmd*
	  (SPACE* 'else' (ws+ ifeq_expr)? ws* NL
		rule_cmd*)*
	  SPACE* 'endif' ws*
;

expr returns [std::string info]:
	  ws* l=atom_lhs ws*
		op=( ':=' | '+=' | '=' | '?=' ) ws*
		r=atom_rhs? ws*
	# ExprAssign
	| SPACE* ifeq_expr ws* NL
		cmd*
	  (SPACE* 'else' (ws+ ifeq_expr)? ws* NL
		cmd*)*
	  SPACE* 'endif' ws*
	# ExprOther
	| SPACE* 'define' atom ('=' | ws)* NL
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
	  '(' atom? COMMA ws* atom? ')'
	| '\'' atom? '\'' ws+ '\'' atom? '\''
	| '"' atom? '"' ws+ '"' atom? '"'
;

atom_lhs returns [std::string cond] :
	  BARE_OBJ		{$cond = $BARE_OBJ.text.back();}
	| (ID
	  | eval		{$cond = $eval.cond;}
	  )+
;

atom_rhs :
	  atoms						// try to eat it as atoms, but:
	| un=nonNL //{std::cout << "UNHANDLED " << $un.text << "\n";}	// slurp anything else
;

atoms returns [std::string cond] :
	(a1=atom {$cond = $a1.cond;})+ (ws+ (a2=atom {$cond = $a2.cond;})+)*
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
	  '$(' in_eval ')'	{$cond = $in_eval.cond;}
	| '${' in_eval '}'	{$cond = $in_eval.cond;}
	| '$@' | '$%' | '$<' | '$?' | '$^' | '$+' | '$|' | '$*'
	| '$' ID
;

in_eval returns [std::string cond] :
	  a1=atom (':' atom? '=' atom?)?		{$cond = $a1.cond;}
	| 'addprefix' ws+ atoms (COMMA atoms?)+
	| 'and' ws+ atoms (COMMA atoms?)*
	| 'call' ws+ atoms (COMMA ('=' | atoms)+)*
	| 'dir' ws+ atoms
	| 'error' ws+ ('or' | atoms | '>' | '=' | '\'')*
	| 'findstring' ws+ atoms COMMA atoms
	| 'filter' ws+ ~COMMA+ COMMA atoms
	| 'filter-out' ws+ ~COMMA+ COMMA ws* atoms
	| 'foreach' ws+ atoms COMMA (':' | atoms)+ COMMA atoms
	| 'if' ws+ atoms COMMA atoms? (COMMA atoms?)?
	| 'notdir' ws+ atoms
	| 'origin' ws+ atoms
	| 'or' ws+ atoms (COMMA atoms?)*
	| 'patsubst' ws+ f=~COMMA+ COMMA t=atoms COMMA e=atoms
	| 'shell' ws+ in_shell+
	| 'sort' ws+ atoms
	| 'subst' ws+ f=~COMMA+ COMMA t=atoms? COMMA e=atoms	{$cond = $e.cond;}
	| 'wildcard' ws+ ('*' | atoms)+
	| 'word' ws+ atoms COMMA atoms
	| 'words' ws+ atoms
	| BITS			{std::cout << "BITS: " << $BITS.line << "\n";}
	| SRCARCH		{std::cout << "SRCA: " << $SRCARCH.line << "\n";}
;

in_shell :
	  '<' | '>' | '2>' | '|' | '='
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
