from unittest.mock import Mock
import pytest
from tests.conftest import Any

from expr import Assign, Binary, Call, Expr, Get, Grouping, Lambda, Literal, Logical, Set, Super, Ternary, This, Unary, Variable
from lox_token import Token
from parser import Parser, ParseException
from scanner import Scanner
from token_type import TokenType


def scan_tokens(source: str) -> list[Token]:
    scanner = Scanner(Mock(), source)
    return  scanner.scan_tokens()


@pytest.mark.parametrize("source, expected_expr_value", [
    ("true", True),
    ("false", False),
    ("nil", None),
    ("123.45", 123.45),
    ('"string"', "string"),
])
def test_expression_parse_literal(source: str, expected_expr_value: object):
    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Literal)
    assert expected_expr_value == actual_expr.value


def test_expression_parse_valid_super():
    source = "super.method"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Super)
    assert TokenType.IDENTIFIER == actual_expr.method.type
    assert "method" == actual_expr.method.lexeme


def test_expression_parse_this():
    source = "this"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, This)


def test_expression_parse_variable():
    source = "var1"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Variable)


def test_expression_parse_grouping():
    source = "(1)"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Grouping)
    assert isinstance(actual_expr.expression, Literal)
    assert actual_expr.expression.value == 1


def test_expression_parse_basic_call():
    source = "f()"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Call)
    assert isinstance(actual_expr.callee, Variable)
    assert 0 == len(actual_expr.arguments)


def test_expression_parse_call_with_arguments():
    source = "f(a = 3, b)"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Call)
    assert isinstance(actual_expr.callee, Variable)
    assert 2 == len(actual_expr.arguments)
    assert isinstance(actual_expr.arguments[0], Assign)
    assert isinstance(actual_expr.arguments[1], Variable)


def test_expression_parse_basic_get():
    source = "object.property"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Get)
    assert "property" == actual_expr.name.lexeme
    assert isinstance(actual_expr.objekt, Variable)


def test_expression_parse_get_call_chain():
    source = "super.parents().method(arg)().property"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Get)
    assert isinstance(actual_expr.objekt, Call)
    assert isinstance(actual_expr.objekt.callee, Call)
    assert isinstance(actual_expr.objekt.callee.callee, Get)
    assert isinstance(actual_expr.objekt.callee.callee.objekt, Call)
    assert isinstance(actual_expr.objekt.callee.callee.objekt.callee, Super)


@pytest.mark.parametrize("source, expected_error", [
    ("super", "Expect '.' after 'super'."),
    ("super.", "Expect superclass method name."),
    ("(1", "Expect ')' after expression."),
    ("", "Expect expression."),
])
def test_expression_parse_error(source: str, expected_error: str):
    mock_reporter = Mock()
    parser = Parser(mock_reporter, scan_tokens(source))
    with pytest.raises(ParseException):
        parser.expression()

    mock_reporter.parse_error.assert_called_with(Any(), expected_error)
