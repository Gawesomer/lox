import pytest

from lox import Lox
from lox_token import Token
from token_type import TokenType
from scanner import Scanner


@pytest.mark.parametrize("source", [
    " \t\r",
    "\n",
    "// line comment",
    "/* block comment */",
    "/* block * comment */",
    "/* block\ncomment */",
])
def test_scan_token_ignored_tokens(source: str):
    scanner = Scanner(Lox, source)
    actual_token = scanner.scan_token()

    assert actual_token is None


@pytest.mark.parametrize("source, expected_token_type", [
    ("!", TokenType.BANG),
    ("!=", TokenType.BANG_EQUAL),
    ('"string"', TokenType.STRING),
    ('"multiline\nstring"', TokenType.STRING),
    ("123", TokenType.NUMBER),
    ("123.45", TokenType.NUMBER),
    ("var1", TokenType.IDENTIFIER),
    ("true", TokenType.TRUE),
])
def test_scan_token_correct_token_type(source: str, expected_token_type: TokenType):
    scanner = Scanner(Lox, source)
    actual_token = scanner.scan_token()

    assert expected_token_type == actual_token.type


@pytest.mark.parametrize("source, expected_token_types", [
    ("", (TokenType.EOF,)),
    (";// line comment\n;", (TokenType.SEMICOLON, TokenType.SEMICOLON, TokenType.EOF)),
])
def test_scan_tokens_correct_token_types(source: str, expected_token_types: list[TokenType]):
    scanner = Scanner(Lox, source)
    actual_tokens = scanner.scan_tokens()

    assert len(expected_token_types) == len(actual_tokens)
    for expected_token_type, actual_token in zip(expected_token_types, actual_tokens):
        assert expected_token_type == actual_token.type
