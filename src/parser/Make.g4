grammar Make;

@header {
#include <string>
}

makefile : cmd* EOF ;

cmd :
	  makeRule				//XX{print(f"RULE {$start.line}: {$makeRule.info}")}
	| ('-include' | 'include') inc=nonNL NL	//{print(f"INC {$start.line}: {$inc.text}")}
	| TAB* 'export' (i=ID | i=CONFIG)+ NL	//{print(f"EXPORT {$start.line}: {$i.text}")}
	| TAB* e=modified_expr NL		//{std::cout << $e.text << "\n";}
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
	modifier? expr				{$info=$expr.info;}
;

makeRule returns [std::string info] :
	  atom {$info=$atom.text;} ':' TAB* e=modified_expr NL		/*{e = $e.text
e = e.split("\n", 2)[0][:100]
print(f"RULE-EXP {$start.line}: {e} --- ({$e.info})")
}*/
	| atom {$info=$atom.text;} ':' (r=nonNL /*{$info += ' RRR ' + $r.text;}*/ )? NL
	    rule_cmd*
;

rule_cmd:
	  (TAB nonNL?)? NL
	| SPACE* ifeq_expr TAB* NL
		rule_cmd*
	  (SPACE* 'else' (ifeq_expr)? TAB* NL
		rule_cmd*)*
	  SPACE* 'endif' TAB*
;

expr returns [std::string info]:
	  TAB* l=atom_lhs TAB*
		op=( ':=' | '+=' | '=' | '?=' ) TAB*
		r=atom_rhs? TAB*
	# ExprAssign
	| SPACE* ifeq_expr TAB* NL
		cmd*
	  (SPACE* 'else' (ifeq_expr)? TAB* NL
		cmd*)*
	  SPACE* 'endif' TAB*
	# ExprOther
	| SPACE* 'define' atom ('=' | TAB)* NL
		(nonNL NL)*?
	  SPACE* 'endef' TAB*
	# ExprOther
	| atom // standalone $(error ...)
	# ExprOther
;

ifeq_expr :
	  ('ifeq' | 'ifneq') ifeq_cond
	| ('ifdef' | 'ifndef') (ID | CONFIG)
;

ifeq_cond :
	  '(' atom? COMMA TAB* atom? ')'
	| '\'' atom? '\'' '\'' atom? '\''
	| '"' atom? '"' '"' atom? '"'
;

atom_lhs returns [std::string cond] :
	  BARE_OBJ		{$cond = $BARE_OBJ.text.back();}
	| (ID
	  | eval		{$cond = $eval.cond;}
	  )+
;

atom_rhs:
	  atom						// try to eat it as atoms, but:
	| un=nonNL //{std::cout << "UNHANDLED " << $un.text << "\n";}	// slurp anything else
;

atom returns [std::string cond] :
	  CONFIG		{$cond = $CONFIG.text;}
	| BARE_OBJ		{$cond = $BARE_OBJ.text.back();}
	| (ID | '%' | '\\#' | TAB | eval)+
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
	| 'addprefix' atom (COMMA atom?)+
	| 'and' atom (COMMA atom?)*
	| 'call' atom (COMMA ('=' | atom)+)*
	| 'dir' atom
	| 'error' ('or' | atom | '>' | '=' | '\'')*
	| 'findstring' atom COMMA atom
	| 'filter' ~COMMA+ COMMA atom
	| 'filter-out' ~COMMA+ COMMA atom+
	| 'foreach' atom COMMA (':' | atom)+ COMMA atom
	| 'if' atom COMMA atom? (COMMA atom?)?
	| 'notdir' atom
	| 'origin' atom
	| 'or' atom (COMMA atom?)*
	| 'patsubst' f=~COMMA+ COMMA t=atom COMMA e=atom
	| 'shell' in_shell+
	| 'sort' atom
	| 'subst' f=~COMMA+ COMMA t=atom? COMMA e=atom	{$cond = $e.cond;}
	| 'wildcard' ('*' | atom)+
	| 'word' atom COMMA atom
	| 'words' atom
	| BITS			{std::cout << "BITS: " << $BITS.line << "\n";}
	| SRCARCH		{std::cout << "SRCA: " << $SRCARCH.line << "\n";}
;

in_shell :
	  '<' | '>' | '2>' | '|' | '='
	| TAB | ID
	| '(' in_shell ')'
	| '$(' in_shell ')'
	| '"' ~'"'* '"'
;

nonNL : ~NL+ ;

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
SPACE : ' ' -> channel(HIDDEN);
OTHER : . ;
