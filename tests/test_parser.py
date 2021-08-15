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
    source = "(1, 2)"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Grouping)
    assert isinstance(actual_expr.expression, Binary)
    assert actual_expr.expression.operator.lexeme == ","


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


def test_expression_parse_single_unary():
    source = "!object.method"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Unary)
    assert TokenType.BANG == actual_expr.operator.type


def test_expression_parse_nested_unary():
    source = "!-1"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Unary)
    assert isinstance(actual_expr.right, Unary)


def test_expression_parse_binary():
    source = "true==3>4"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Binary)
    assert TokenType.EQUAL_EQUAL == actual_expr.operator.type
    assert isinstance(actual_expr.left, Literal)
    assert isinstance(actual_expr.right, Binary)
    assert TokenType.GREATER == actual_expr.right.operator.type


def test_expression_parse_multiple_binaries():
    source = "1==2!=3"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert actual_expr.left.left.value == 1
    assert actual_expr.left.right.value == 2
    assert actual_expr.right.value == 3


def test_expression_parse_logical_and():
    source = "1==2 and 2!=3"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Logical)
    assert TokenType.EQUAL_EQUAL == actual_expr.left.operator.type
    assert TokenType.BANG_EQUAL == actual_expr.right.operator.type


def test_expression_parse_logical_or():
    source = "1==2 or true and false"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Logical)
    assert TokenType.OR == actual_expr.operator.type
    assert isinstance(actual_expr.left, Binary)
    assert isinstance(actual_expr.right, Logical)


def test_expression_parse_ternary():
    source = "1 or 2 ? 3 or 4 : 5 or 6"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Ternary)
    assert isinstance(actual_expr.conditional, Logical)
    assert isinstance(actual_expr.truthy, Logical)
    assert isinstance(actual_expr.falsy, Logical)


def test_expression_parse_nested_ternaries():
    source = "1 ? 2?3:4 : 5?6:7"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Ternary)
    assert 1 == actual_expr.conditional.value
    assert isinstance(actual_expr.truthy, Ternary)
    assert 2 == actual_expr.truthy.conditional.value
    assert isinstance(actual_expr.falsy, Ternary)
    assert 5 == actual_expr.falsy.conditional.value


def test_expression_parse_assignment():
    source = "a = 1?2:3"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Assign)
    assert "a" == actual_expr.name.lexeme
    assert isinstance(actual_expr.value, Ternary)


def test_expression_parse_set():
    source = "object.method = 1?2:3"

    parser = Parser(Mock(), scan_tokens(source))
    actual_expr = parser.expression()

    assert isinstance(actual_expr, Set)
    assert isinstance(actual_expr.objekt, Variable)
    assert isinstance(actual_expr.value, Ternary)


@pytest.mark.parametrize("source, expected_error", [
    ("", "Expect expression."),
    ("super", "Expect '.' after 'super'."),
    ("super.", "Expect superclass method name."),
    ("(1", "Expect ')' after expression."),
    ("object.", "Expect property name after '.'."),
    ("f(arg", "Expect ')' after arguments."),
    ("true?false", "Expect ':'."),
    ("?true:false", "Expect expression."),
])
def test_expression_parse_raises_error(source: str, expected_error: str):
    mock_reporter = Mock()
    parser = Parser(mock_reporter, scan_tokens(source))
    with pytest.raises(ParseException):
        parser.expression()

    mock_reporter.parse_error.assert_called_with(Any(), expected_error)


@pytest.mark.parametrize("source, expected_error", [
    ("f({})".format(','.join([str(i) for i in range(256)])), "Can't have more than 255 arguments."),
    ("!=3>4 expr", "Equality operator without left-hand operand."),
    ("?true expr", "Ternary operator without left-hand operand."),
    (":true expr", "Ternary operator without left-hand operand."),
])
def test_expression_parse_reports_error(source: str, expected_error: str):
    mock_reporter = Mock()
    parser = Parser(mock_reporter, scan_tokens(source))
    parser.expression()

    mock_reporter.parse_error.assert_called_with(Any(), expected_error)
