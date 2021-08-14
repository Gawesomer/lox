from unittest.mock import Mock
import pytest
from tests.conftest import Any

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
    scanner = Scanner(Mock(), source)
    actual_token = scanner.scan_token()

    assert actual_token is None


@pytest.mark.parametrize("source, expected_token_type", [
    ("!", TokenType.BANG),
    ("!=", TokenType.BANG_EQUAL),
    ('"string"', TokenType.STRING),
    ('"multiline\nstring"', TokenType.STRING),
    ("123", TokenType.NUMBER),
    ("123.45", TokenType.NUMBER),
    ("123.", TokenType.NUMBER),
    ("var1", TokenType.IDENTIFIER),
    ("true", TokenType.TRUE),
])
def test_scan_token_correct_token_type(source: str, expected_token_type: TokenType):
    scanner = Scanner(Mock(), source)
    actual_token = scanner.scan_token()

    assert expected_token_type == actual_token.type


@pytest.mark.parametrize("source, expected_token_literal", [
    ('"string"', "string"),
    ('"multiline\nstring"', "multiline\nstring"),
    ("123", float(123)),
    ("123.45", 123.45),
    ("123.", float(123.0)),
])
def test_scan_token_correct_literal(source: str, expected_token_literal: object):
    scanner = Scanner(Mock(), source)
    actual_token = scanner.scan_token()

    assert expected_token_literal == actual_token.literal


@pytest.mark.parametrize("source, expected_token_types", [
    ("", (TokenType.EOF,)),
    ("<==", (TokenType.LESS_EQUAL, TokenType.EQUAL, TokenType.EOF)),
    (";// line comment\n;", (TokenType.SEMICOLON, TokenType.SEMICOLON, TokenType.EOF)),
])
def test_scan_tokens_correct_token_types(source: str, expected_token_types: list[TokenType]):
    scanner = Scanner(Mock(), source)
    actual_tokens = scanner.scan_tokens()

    assert len(expected_token_types) == len(actual_tokens)
    for expected_token_type, actual_token in zip(expected_token_types, actual_tokens):
        assert expected_token_type == actual_token.type


@pytest.mark.parametrize("source, expected_error", [
    ("$", "Unexpected character."),
    ('"unterminated string', "Unterminated string."),
    ("/* unterminated block comment", "Unterminated block comment."),
])
def test_scan_token_invalid_character(source: str, expected_error: str):
    mock_reporter = Mock()
    scanner = Scanner(mock_reporter, source)
    scanned_token = scanner.scan_token()

    assert scanned_token is None
    mock_reporter.error.assert_called_with(Any(), expected_error)
