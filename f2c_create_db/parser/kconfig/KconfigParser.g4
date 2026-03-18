// SPDX-License-Identifier: GPL-2.0-only

parser grammar KconfigParser;

options { tokenVocab=KconfigLexer; }

kbuild : cmdNL* cmd? EOF ;

cmdNL : cmd? NL ;

cmd :
	  source
	| cond_cmd
	| config
	| comment
	| choice
	| menu
	| mainmenu
;

expr :
	  ID
	| STRING
	| integer
	| LPAREN expr RPAREN
	| Triple
	| NOT expr
	| expr AND expr
	| expr OR expr
	| expr comp expr
	| ExtCmd ec_content+ EC_END
;

ec_content :
	  EC_CONTENT
	| EC_STRING
	| EC_LPAREN ec_content+ EC_RPAREN
;

comp : GT | GEQ | LT | LEQ | EQ | NE ;

str_or_src :
	  STRING
	| SOURCE // old kernels allowed unquoted: source a/b/c.d
;

source : Source str_or_src ;

cond_cmd :
	cond NL
	cmdNL*
	Endif
;

cond : If expr ;

config_id :
	  ID
	| integer
;

config : (Menuconfig | Config) config_id
	(NL config_line?)*
;

config_type :
	  Bool | Tristate | Int | Hex | String
	| Def_bool | Def_tristate
;

config_line:
	  type=config_type expr? cond?
	| prompt
	| Transitional
	| default
	| ( Select | Imply ) config_id cond?
	| Range expr expr cond?
	| Modules
	| Option (Modules | ID) (EQ expr)?
	| depends_on
	| help
;

prompt :
	Prompt STRING cond?
;

default :
	// old kernels: SOURCE
	Default (expr | SOURCE) cond?
;

depends_on :
	Depends_on expr
;

help :
	Help help_content
;

help_content :
	(HLP_WS | HLP_NL | HLP_TEXT)*
;

comment :
	Comment STRING
	(NL depends_on)*
;

choice :
	Choice ID? // old kernels: ID
	(choice_line? NL)*
	cmdNL*
	Endchoice
;

choice_line:
	  config_type STRING // old kernels
	| prompt
	| default
	| depends_on
	| Optional
	| help
;

menu :
	Menu STRING
	(menu_line? NL)*
	cmdNL*
	Endmenu
;

menu_line:
	  Visible cond?
	| Depends_on expr
;

mainmenu :
	Mainmenu STRING
;

integer : HEX | INT ;
