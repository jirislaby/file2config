// SPDX-License-Identifier: GPL-2.0-only

parser grammar KconfigParser;

options { tokenVocab=KconfigLexer; }

kbuild : cmdNL* EOF ;

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

source : Source STRING ;

cond_cmd :
	cond NL
	cmdNL*
	Endif
;

cond : If expr ;

config : (Menuconfig | Config) ID
	(NL config_line?)*
;

config_line:
	  type=(Bool | Tristate | Int | Hex | String) STRING? cond?
	| prompt
	| Transitional
	| default
	| type=(Def_bool | Def_tristate) expr cond?
	| ( Select | Imply ) ID cond?
	| Range expr expr cond?
	| Modules
	| depends_on
	| help
;

prompt :
	Prompt STRING cond?
;

default :
	Default expr cond?
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
	Choice
	(choice_line? NL)*
	cmdNL*
	Endchoice
;

choice_line:
	  prompt
	| default
	| depends_on
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
