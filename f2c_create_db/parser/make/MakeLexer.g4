// SPDX-License-Identifier: GPL-2.0-only

lexer grammar MakeLexer;

HASH : '\\#' ;

MULTILINE_COMMENT :
	'#' COMMENT_BODY (CONT_LINE COMMENT_BODY)*
	-> skip
;
fragment COMMENT_BODY : ~('\n' | '\\')* ;
CONT_LINE : '\\' ' '* NL -> skip;

ERROR : EVALPAR ('error'|'warning'|'info') WS SKIP_BODY RPAREN -> skip ;
SHELL : EVALPAR 'shell' WS SKIP_BODY RPAREN -> skip ;
fragment SKIP_BODY :
	  ( ~[()] | LPAREN SKIP_BODY RPAREN )*
;

INCLUDE : '-'? 'include' ;
EXPORT : 'export' ;
UNEXPORT : 'unexport' ;

OVERRIDE : 'override' ;
PRIVATE : 'private' ;
FORCE : 'FORCE' ;

IFEQ : 'ifeq' ;
IFNEQ : 'ifneq' ;
IFDEF : 'ifdef' ;
IFNDEF : 'ifndef' ;
ELSE : 'else' ;
ENDIF : 'endif' ;

DEFINE : 'define' ;
ENDEF : 'endef' ;
UNDEFINE : 'undefine' ;

FUN_ABSPATH :		'$(abspath' WS ;
FUN_ADDPREFIX :		'$(addprefix' WS ;
FUN_ADDSUFFIX :		'$(addsuffix' WS ;
FUN_AND :		'$(and' WS ;
FUN_BASENAME :		'$(basename' WS ;
FUN_CALL :		'$(call' WS ;
FUN_DIR : 		'$(dir' WS ;
FUN_EVAL :		'$(eval' WS ;
FUN_FILE :		'$(file' WS ;
FUN_FILTER :		'$(filter' WS ;
FUN_FILTER_OUT :	'$(filter-out' WS ;
FUN_FINDSTRING :	'$(findstring' WS ;
FUN_FIRSTWORD :		'$(firstword' WS ;
FUN_FLAVOR :		'$(flavor' WS ;
FUN_FOREACH :		'$(foreach' WS ;
FUN_IF :		'$(if' WS ;
FUN_INTCMP :		'$(intcmp' WS ;
FUN_JOIN :		'$(join' WS ;
FUN_LASTWORD :		'$(lastword' WS ;
FUN_LET :		'$(let' WS ;
FUN_NOTDIR :		'$(notdir' WS ;
FUN_OR : 		'$(or' WS ;
FUN_ORIGIN :		'$(origin' WS ;
FUN_PATSUBST :		'$(patsubst' WS ;
FUN_REALPATH :		'$(realpath' WS ;
FUN_SORT :		'$(sort' WS ;
FUN_STRIP :		'$(strip' WS ;
FUN_SUBST :		'$(subst' WS ;
FUN_SUFFIX :		'$(suffix' WS ;
FUN_VALUE :		'$(value' WS ;
FUN_WILDCARD :		'$(wildcard' WS ;
FUN_WORD :		'$(word' WS ;
FUN_WORDLIST :		'$(wordlist' WS ;
FUN_WORDS :		'$(words' WS ;

EVALPAR : '$(' ;
EVALBRK : '${' ;

DOLL_BUILTIN : '$@' | '$%' | '$<' | '$?' | '$^' | '$+' | '$|' | '$*' | '$$' ;
DOLL_ID : '$' ID ;

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

ASSIGN : ':=' | '?=' ;
APPEND : '+=' ;

EQ : '=' ;
LPAREN : '(' ;
RPAREN : ')' ;
RBRK : '}' ;
LT : '<' ;
GT : '>' ;
SHR : '>>' ;
ASTERISK : '*' ;
EXCLAMATION : '!' ;
PERCENT : '%' ;

RULE_SEP: '::' | '&:' ;
COLON : ':' ;
COMMA : ',' ;
ESC : '\\' . ;

STRING:
	  '"' ~'"'* '"'
	| '\'' ~'\''* '\''
	;

NL : '\r'? '\n' ;
fragment WS : SPACE | TAB ;
TAB : '\t' ;
SPACE : ' ' ;

OTHER : .  ;
