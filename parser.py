from expr import Assign, Binary, Call, Expr, Get, Grouping, Lambda, Literal, Logical, Set, Ternary, This, Unary, Variable
from stmt import Block, Break, Class, Expression, Function, If, Print, Return, Stmt, Var, While
from lox_token import Token
from token_type import TokenType


class Parser:
    """
    Expression grammar:
        program        → declaration* EOF ;
        declaration    → class_decl
                       | fun_decl
                       | var_decl
                       | statement ;
        class_decl     → "class" IDENTIFIER "{" ( "class"? function )* "}" ;
        class_body     → ( "class"? function | IDENTIFIER block )* ;
        fun_decl       → "fun" IDENTIFIER function ;
        method         → IDENTIFIER "(" parameters? ")" block ;
        function       → "(" parameters? ")" block ;
        parameters     → IDENTIFIER ( "," IDENTIFIER )* ;
        var_decl       → "var" IDENTIFIER ( "=" expression )? ";" ;
        statement      → expr_stmt
                       | for_stmt
                       | if_stmt
                       | print_stmt
                       | return_stmt
                       | while_stmt
                       | break_stmt
                       | block ;
        expr_stmt      → expression ";" ;
        for_stmt       → "for" "(" ( var_decl | expr_stmt | ";" )
                         expression? ";"
                         expression? ")" statement ;
        if_stmt        → "if" "(" expression ")" statement
                       ( "else" statement )? ;
        print_stmt     → "print" expression ";" ;
        return_stmt    → "return" expression? ";" ;
        while_stmt     → "while" "(" expression ")" statement ;
        break_stmt     → "break" ";" ;
        block          → "{" declaration* "}" ;
        expression     → inv_comma ;
        inv_comma      → "," comma ;
        comma          → assignment ( "," assignment )* ;
        assignment     → ( call "." )? IDENTIFIER "=" assignment
                       | inv_ternary
                       | lambda ;
        lambda         → "fun" function ;
        inv_ternary    → ( "?" | ":" ) ternary ;
        ternary        → logic_or ( "?" ternary ":" ternary )* ;
        logic_or       → logic_and ( "or" logic_and )* ;
        logic_and      → inv_equality ( "and" inv_equality )* ;
        inv_equality   → ( "==" | "!=" ) equality ;
        equality       → inv_comparison ( ( "!=" | "==" ) inv_comparison )* ;
        inv_comparison → ( ">" | ">=" | "<" | "<=" ) comparison ;
        comparison     → inv_term ( ( ">" | ">=" | "<" | "<=" ) inv_term )* ;
        inv_term       → "+" term ;
        term           → inv_factor ( ( "-" | "+" ) inv_factor )* ;
        inv_factor     → ( "/" | "*" ) factory ;
        factor         → unary ( ( "/" | "*" ) unary )* ;
        unary          → ( "!" | "-" ) unary | call ;
        call           → primary ( "(" arguments? ")" | "." IDENTIFIER )* ;
        arguments      → assignment ( "," assignment )* ;
        primary        → NUMBER | STRING | "true" | "false" | "nil"
                       | "(" expression ")"
                       | IDENTIFIER ;
    """

    class ParseException(Exception):
        pass

    def __init__(self, reporter: "Lox", tokens: list[Token]):
        self.reporter = reporter
        self.tokens = tokens
        self.current = 0

    def parse(self) -> list[Stmt]:
        statements = []
        while not self.is_at_end():
            statements.append(self.declaration())

        return statements

    def declaration(self) -> Stmt:
        try:
            if self.match(TokenType.CLASS):
                return self.class_declaration()
            if self.check(TokenType.FUN) and not self.check_next(TokenType.LEFT_PAREN):
                self.advance()  # Consume "fun" token.
                return self.function_declaration("function")
            if self.match(TokenType.VAR):
                return self.var_declaration()
            return self.statement()
        except self.ParseException:
            self.synchronize()
            return None

    def class_declaration(self) -> Stmt:
        name = self.consume(TokenType.IDENTIFIER, "Expect class name.")
        self.consume(TokenType.LEFT_BRACE, "Expect '{' before class body.")

        class_methods = []
        instance_methods = []
        getters = []
        while not self.check(TokenType.RIGHT_BRACE) and not self.is_at_end():
            if self.match(TokenType.CLASS):
                class_methods.append(self.function_declaration("method"))
            elif self.check_next(TokenType.LEFT_PAREN):
                instance_methods.append(self.function_declaration("method"))
            else:
                getter_name = self.consume(TokenType.IDENTIFIER, "Expect getter name.")
                self.consume(TokenType.LEFT_BRACE, "Expect '{}' before the getter body.")
                body = self.block()
                getters.append(Function(getter_name, (), body))

        self.consume(TokenType.RIGHT_BRACE, "Expect '}' after class body.")

        return Class(name, class_methods, instance_methods, getters)

    def function_declaration(self, kind: str) -> Stmt:
        name = self.consume(TokenType.IDENTIFIER, "Expect {} name.".format(kind))
        parameters, body = self.function(kind)
        return Function(name, parameters, body)

    def function(self, kind: str) -> (list[Token], list[Stmt]):
        self.consume(TokenType.LEFT_PAREN, "Expect '(' after {} declaration.".format(kind))
        parameters = []
        if not self.check(TokenType.RIGHT_PAREN):
            parameters.append(self.consume(TokenType.IDENTIFIER, "Expect parameter name."))
            while self.match(TokenType.COMMA):
                if len(parameters) >= 255:
                    self.error(self.peek(), "Can't have more than 255 parameters.")
                parameters.append(self.consume(TokenType.IDENTIFIER, "Expect parameter name."))
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after parameters.")

        self.consume(TokenType.LEFT_BRACE, "Expect '{}' before the {} body.".format('{', kind))
        body = self.block()
        return (parameters, body)

    def var_declaration(self) -> Stmt:
        name = self.consume(TokenType.IDENTIFIER, "Expect variable name.")

        initializer = None
        if self.match(TokenType.EQUAL):
            initializer = self.expression()

        self.consume(TokenType.SEMICOLON, "Expect ';' after variable declaration.")
        return Var(name, initializer)

    def statement(self) -> Stmt:
        if self.match(TokenType.FOR):
            return self.for_statement()
        if self.match(TokenType.IF):
            return self.if_statement()
        if self.match(TokenType.PRINT):
            return self.print_statement()
        if self.match(TokenType.RETURN):
            return self.return_statement()
        if self.match(TokenType.WHILE):
            return self.while_statement()
        if self.match(TokenType.BREAK):
            return self.break_statement()
        if self.match(TokenType.LEFT_BRACE):
            return Block(self.block())

        return self.expression_statement()

    def for_statement(self) -> Stmt:
        self.consume(TokenType.LEFT_PAREN, "Expect '(' after 'for'.")

        initializer = None
        if self.match(TokenType.SEMICOLON):
            initializer = None
        elif self.match(TokenType.VAR):
            initializer = self.var_declaration()
        else:
            initializer = self.expression_statement()

        condition = None
        if not self.check(TokenType.SEMICOLON):
            condition = self.expression()
        self.consume(TokenType.SEMICOLON, "Expect ';' after loop condition.")

        increment = None
        if not self.check(TokenType.RIGHT_PAREN):
            increment = self.expression()
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after for clauses.")

        body = self.statement();

        if increment is not None:
            body = Block([body, Expression(increment)])

        if condition is None:
            condition = Literal(True)
        body = While(condition, body)

        if initializer is not None:
            body = Block([initializer, body])

        return body

    def if_statement(self) -> Stmt:
        self.consume(TokenType.LEFT_PAREN, "Expect '(' after 'if'.")
        condition = self.expression()
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after if condition.")

        then_branch = self.statement()
        else_branch = None
        if self.match(TokenType.ELSE):
            else_branch = self.statement()

        return If(condition, then_branch, else_branch)

    def print_statement(self) -> Stmt:
        value = self.expression()
        self.consume(TokenType.SEMICOLON, "Expect ';' after value.")
        return Print(value)

    def return_statement(self) -> Stmt:
        keyword = self.previous()
        value = None
        if not self.check(TokenType.SEMICOLON):
            value = self.expression()

        self.consume(TokenType.SEMICOLON, "Expect ';' after return value.")
        return Return(keyword, value)

    def while_statement(self) -> Stmt:
        self.consume(TokenType.LEFT_PAREN, "Expect '(' after 'while'.")
        condition = self.expression()
        self.consume(TokenType.RIGHT_PAREN, "Expect ')' after while condition.")
        body = self.statement()

        return While(condition, body)

    def break_statement(self) -> Stmt:
        break_stmt = Break(self.previous())
        self.consume(TokenType.SEMICOLON, "Expect ';' after break.")
        return break_stmt

    def block(self) -> list[Stmt]:
        statements = []

        while not self.check(TokenType.RIGHT_BRACE) and not self.is_at_end():
            statements.append(self.declaration())

        self.consume(TokenType.RIGHT_BRACE, "Expect '}' after block.")
        return statements

    def expression_statement(self) -> Stmt:
        expr = self.expression()
        self.consume(TokenType.SEMICOLON, "Expect ';' after expression.")
        return Expression(expr)

    def expression(self) -> Expr:
        return self.inv_comma()

    def inv_comma(self) -> Expr:
        if self.match(TokenType.COMMA):
            self.error(self.peek(), "Comma operator without left-hand operand.")
            invalid_expression = self.comma()

        return self.comma()

    def comma(self) -> Expr:
        expression = self.assignment()

        while self.match(TokenType.COMMA):
            operator = self.previous()
            right = self.assignment()
            expression = Binary(expression, operator, right)

        return expression

    def assignment(self) -> Expr:
        if self.match(TokenType.FUN):
            parameters, body = self.function("function")
            return Lambda(parameters, body)

        expr = self.inv_ternary()

        if self.match(TokenType.EQUAL):
            equals = self.previous()
            value = self.assignment()

            if isinstance(expr, Variable):
                name = expr.name
                return Assign(name, value)
            elif isinstance(expr, Get):
                return Set(expr.objekt, expr.name, value)

            self.error(equals, "Invalid assignment target.")

        return expr

    def inv_ternary(self) -> Expr:
        if self.match(TokenType.EROTEME, TokenType.COLON):
            self.error(self.peek(), "Ternary operator without left-hand operand.")
            invalid_expression = self.ternary()

        return self.ternary()

    def ternary(self) -> Expr:
        expression = self.logic_or()

        while self.match(TokenType.EROTEME):
            truthy = self.ternary()
            if self.match(TokenType.COLON):
                falsy = self.ternary()
                expression = Ternary(expression, truthy, falsy)
            else:
                raise self.error(self.peek(), "Expect ':'.")

        return expression

    def logic_or(self) -> Expr:
        expr = self.logic_and()

        while self.match(TokenType.OR):
            operator = self.previous()
            right = self.logic_and()
            expr = Logical(expr, operator, right)

        return expr

    def logic_and(self) -> Expr:
        expr = self.inv_equality()

        while self.match(TokenType.AND):
            operator = self.previous()
            right = self.inv_equality()
            expr = Logical(expr, operator, right)

        return expr

    def inv_equality(self) -> Expr:
        if self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            self.error(self.peek(), "Equality operator without left-hand operand.")
            invalid_expression = self.equality()

        return self.equality()

    def equality(self) -> Expr:
        expression = self.inv_comparison()

        while self.match(TokenType.BANG_EQUAL, TokenType.EQUAL_EQUAL):
            operator = self.previous()
            right = self.inv_comparison()
            expression = Binary(expression, operator, right)

        return expression

    def inv_comparison(self) -> Expr:
        if self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            self.error(self.peek(), "Comparison operator without left-hand operand.")
            invalid_expression = self.comparison()

        return self.comparison()

    def comparison(self) -> Expr:
        expression = self.inv_term()

        while self.match(TokenType.GREATER, TokenType.GREATER_EQUAL, TokenType.LESS, TokenType.LESS_EQUAL):
            operator = self.previous()
            right = self.inv_term()
            expression = Binary(expression, operator, right)

        return expression

    def inv_term(self) -> Expr:
        if self.match(TokenType.PLUS):
            self.error(self.peek(), "Term operator without left-hand operand.")
            invalid_expression = self.term()

        return self.term()

    def term(self) -> Expr:
        expression = self.inv_factor()

        while self.match(TokenType.MINUS, TokenType.PLUS):
            operator = self.previous()
            right = self.inv_factor()
            expression = Binary(expression, operator, right)

        return expression

    def inv_factor(self) -> Expr:
        if self.match(TokenType.SLASH, TokenType.STAR):
            self.error(self.peek(), "Factor operator without left-hand operand.")
            invalid_expression = self.factor()

        return self.factor()

    def factor(self) -> Expr:
        expression = self.unary()

        while self.match(TokenType.SLASH, TokenType.STAR):
            operator = self.previous()
            right = self.factor()
            expression = Binary(expression, operator, right)

        return expression

    def unary(self) -> Expr:
        if self.match(TokenType.BANG, TokenType.MINUS):
            operator = self.previous()
            right = self.unary()
            return Unary(operator, right)

        return self.call()

    def call(self) -> Expr:
        expr = self.primary()

        while True:
            if self.match(TokenType.LEFT_PAREN):
                expr = self.finish_call(expr)
            elif self.match(TokenType.DOT):
                name = self.consume(TokenType.IDENTIFIER, "Expect property name after '.'.")
                expr = Get(expr, name)
            else:
                break

        return expr

    def finish_call(self, callee: Expr) -> Expr:
        arguments = []
        if not self.check(TokenType.RIGHT_PAREN):
            arguments.append(self.assignment())
            while self.match(TokenType.COMMA):
                if len(arguments) >= 255:
                    self.error(self.peek(), "Can't have more than 255 arguments.")
                arguments.append(self.assignment())

        paren = self.consume(TokenType.RIGHT_PAREN, "Expect ')' after arguments.")

        return Call(callee, paren, arguments)

    def primary(self) -> Expr:
        if self.match(TokenType.FALSE):
            return Literal(False)
        if self.match(TokenType.TRUE):
            return Literal(True)
        if self.match(TokenType.NIL):
            return Literal(None)

        if self.match(TokenType.NUMBER, TokenType.STRING):
            return Literal(self.previous().literal)

        if self.match(TokenType.THIS):
            return This(self.previous())

        if self.match(TokenType.IDENTIFIER):
            return Variable(self.previous())

        if self.match(TokenType.LEFT_PAREN):
            expression = self.expression()
            self.consume(TokenType.RIGHT_PAREN, "Expect ')' after expression.")
            return Grouping(expression)

        raise self.error(self.peek(), "Expect expression.")

    def match(self, *types) -> bool:
        for type_ in types:
            if self.check(type_):
                self.advance()
                return True

        return False

    def consume(self, token_type: TokenType, message: str) -> Token:
        if self.check(token_type):
            return self.advance()

        raise self.error(self.peek(), message)

    def check(self, token_type: TokenType) -> bool:
        if self.is_at_end():
            return False
        return self.peek().type == token_type

    def check_next(self, token_type: TokenType) -> bool:
        if self.current+1 >= len(self.tokens):
            return False
        return self.tokens[self.current+1].type == token_type

    def advance(self) -> Token:
        if not self.is_at_end():
            self.current += 1
        return self.previous()

    def is_at_end(self) -> bool:
        return self.peek().type == TokenType.EOF

    def peek(self) -> Token:
        return self.tokens[self.current]

    def previous(self) -> Token:
        return self.tokens[self.current-1]

    def error(self, token: Token, message: str) -> Exception:
        self.reporter.parse_error(token, message)
        return self.ParseException()

    def synchronize(self):
        self.advance()

        while not self.is_at_end():
            if self.previous().type == TokenType.SEMICOLON:
                return

            if self.peek().type in [
                    TokenType.CLASS,
                    TokenType.FUN,
                    TokenType.VAR,
                    TokenType.FOR,
                    TokenType.IF,
                    TokenType.WHILE,
                    TokenType.PRINT,
                    TokenType.RETURN]:
                return

            self.advance()
