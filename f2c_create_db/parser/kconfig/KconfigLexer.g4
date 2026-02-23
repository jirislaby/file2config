lexer grammar KconfigLexer;

@header {
#include <sl/helpers/Misc.h>
#include <sl/helpers/String.h>
}

@members {

unsigned getDebug() {
	auto debug = 0U;
	if (auto debugOpt = SlHelpers::Env::get("KC_LEXER_DEBUG"))
		if (auto debugNum = SlHelpers::String::toNum(*debugOpt))
			debug = *debugNum;
	return debug;
}

}

@declarations {

enum : unsigned {
	D_HLP	= 1 << 0,
	D_STR	= 1 << 1,
	D_INDNT	= 1 << 2,
	D_EC	= 1 << 5,
};

unsigned m_deb = getDebug();
unsigned m_hlpIndentFirst;
unsigned m_hlpIndentLast;
unsigned m_ecNest;

unsigned getIndent(std::string_view text) {
	unsigned indent = 0;
	for (const auto &c: text) {
		if (c == ' ')
			indent++;
		else if (c == '\t')
			indent = (indent & ~7) + 8;
		else
			break;
	}
	return indent;
}

bool checkIndent(std::string_view text) {
	m_hlpIndentLast = getIndent(getText());
	deb(D_INDNT, "T(", text, ")=", m_hlpIndentLast, " < ", m_hlpIndentFirst);
	return m_hlpIndentFirst && m_hlpIndentLast < m_hlpIndentFirst;
}

bool nextCharIsNotWhitespace() {
	auto nextChar = _input->LA(1);
	return nextChar != ' ' && nextChar != '\t' && nextChar != '\n' && nextChar != '\r';
}

template <typename... Args>
void deb(unsigned level, Args&&... args) {
	if (level & m_deb)
		(std::cout << ... << std::forward<Args>(args)) << '\n';
}

}

COMMENT : '#' ~'\n'* -> skip ;

CONT_LINE : '\\' ' '* NL -> skip ;

LPAREN : '(' ;
RPAREN : ')' ;
AND : '&&' ;
OR  : '||' ;
NOT : '!' ;

GT  : '>' ;
GEQ : '>=' ;
LT  : '<' ;
LEQ : '<=' ;
EQ  : '=' ;
NE  : '!=' ;

Source	: 'source' ;
If	: 'if' ;
Endif	: 'endif' ;

Menuconfig : 'menuconfig' ;
Config : 'config' ;

Bool : 'bool' ;
Tristate : 'tristate' ;
Def_bool : 'def_bool' ;
Def_tristate : 'def_tristate' ;
Modules : 'modules' ;
Transitional : 'transitional' ;
Visible : 'visible' ;

Int : 'int' ;
Hex : 'hex' ;
String : 'string' ;
Range : 'range' ;

Select : 'select' ;
Imply : 'imply' ;

Prompt : 'prompt' ;
Default : 'default' ;
Depends_on : 'depends on' ;

Comment : 'comment' ;

Choice : 'choice' ;
Endchoice : 'endchoice' ;

Menu : 'menu' ;
Endmenu : 'endmenu' ;

Mainmenu : 'mainmenu' ;

STRING :
	  '"' ('\\"' | ~'"')*? '"' { deb(D_STR, "STRING=(", getLine(), ')', getText()); }
	| '\'' ~'\''*? '\''
;

Help :
	[\t ]* 'help' NL { m_hlpIndentFirst = m_hlpIndentLast = 0; }
		-> mode(HELP_MODE)
;

ExtCmd : '$(' { m_ecNest = 0; deb(D_EC, "ExtCmd ln=", getLine()); } -> mode(EXT_CMD_MODE) ;

HEX : '0x' (OneINT | [a-fA-F])+ ;
INT : '-'? OneINT+ ;
fragment OneINT : [0-9] ;

Triple : [ymn] ;

ID : ([A-Za-z_] | OneINT)+ ;

NL : '\r'? '\n' ;

WS : [ \t]+ -> skip ;

// ====== Help mode ======

mode HELP_MODE;

HLP_WS_END :
	[ \t]+ { checkIndent(getText()) }? { deb(D_HLP, getLine(), " DEFAULT1"); }
		-> mode(DEFAULT_MODE),type(NL)
;

HLP_WS :
	[ \t]+
;

HLP_NL_END :
	[ \t]* '\n' { nextCharIsNotWhitespace() }? { deb(D_HLP, getLine(), " DEFAULT2"); }
		-> mode(DEFAULT_MODE),type(NL)
;

HLP_NL :
	[ \t]* '\n' { deb(D_HLP, getLine(), " EMPTY eaten"); }
;

HLP_TEXT :
	~[ \t\n] ~'\n'* {
		deb(D_HLP, getLine(), " OTHER eaten");
		if (!m_hlpIndentFirst)
			m_hlpIndentFirst = m_hlpIndentLast;
	}
;

// ====== Ext cmd mode ======

mode EXT_CMD_MODE;

EC_STRING : '\'' ~'\''*? '\'' ;

EC_LPAREN : '(' { m_ecNest++; deb(32, "INC ", m_ecNest); } ;
EC_RPAREN : ')' { m_ecNest }? { m_ecNest--; deb(32, "DEC ", m_ecNest); } ;
EC_END    : ')' { deb(32, "DEFAULT"); } -> mode(DEFAULT_MODE) ;

EC_CONTENT : ~[()']+ { deb(32, "consume ", getLine(), ' ', getText()); } ;
