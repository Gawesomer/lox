#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

struct Scanner {
	const char *start;
	const char *current;
	int line;
};

struct Scanner scanner;

void init_scanner(const char *source)
{
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}

static bool is_at_end(void)
{
	return *scanner.current == '\0';
}

static char advance(void)
{
	scanner.current++;
	return scanner.current[-1];
}

static bool match(char expected)
{
	if (is_at_end())
		return false;
	if (*scanner.current != expected)
		return false;
	scanner.current++;
	return true;
}

static struct Token make_token(enum TokenType type)
{
	struct Token token;

	token.type = type;
	token.start = scanner.start;
	token.length = (int)(scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

static struct Token error_token(const char *message)
{
	struct Token token;

	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner.line;
	return token;
}

struct Token scan_token(void)
{
	scanner.start = scanner.current;

	if (is_at_end())
		return make_token(TOKEN_EOF);

	char c = advance();

	switch (c) {
	case '(':
		return make_token(TOKEN_LEFT_PAREN);
	case ')':
		return make_token(TOKEN_RIGHT_PAREN);
	case '{':
		return make_token(TOKEN_LEFT_BRACE);
	case '}':
		return make_token(TOKEN_RIGHT_BRACE);
	case ';':
		return make_token(TOKEN_SEMICOLON);
	case ',':
		return make_token(TOKEN_COMMA);
	case '.':
		return make_token(TOKEN_DOT);
	case '-':
		return make_token(TOKEN_MINUS);
	case '+':
		return make_token(TOKEN_PLUS);
	case '/':
		return make_token(TOKEN_SLASH);
	case '*':
		return make_token(TOKEN_STAR);
	case '!':
		return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
	case '=':
		return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
	case '<':
		return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
	case '>':
		return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
	}

	return error_token("Unexpected character.");
}
