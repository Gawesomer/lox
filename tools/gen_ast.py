import sys


class GenerateAST:

    @classmethod
    def main(cls):
        if len(sys.argv) != 2:
            print("Usage: generate_ast <output directory>")
            exit(1)
        output_dir = sys.argv[1]
        cls.define_ast(output_dir, "Expr", [
            "Binary   ; left: Expr, operator: Token, right: Expr",
            "Grouping ; expression: Expr",
            "Literal  ; value: object",
            "Unary    ; operator: Token, right: Expr"
        ])


    @classmethod
    def define_ast(cls, output_dir: str, base_name: str, types: list[str]):
        path = "{}/{}.py".format(output_dir, base_name.lower())
        with open(path, "w") as writer:
            # Imports.
            writer.write("from token import Token\n")
            writer.write("\n")
            writer.write("\n")

            # Base class.
            writer.write("class {}:\n".format(base_name))

            cls.define_visitor(writer, base_name, types)

            # The base accept() method.
            writer.write("    def accept(self, visitor):\n")
            writer.write("        raise NotImplementedError\n")

            writer.write("\n")
            writer.write("\n")

            # The AST classes.
            for type_ in types:
                class_name, fields = [s.strip() for s in type_.split(";")]
                cls.define_type(writer, base_name, class_name, fields)


    @classmethod
    def define_type(cls, writer, base_name: str, class_name: str, field_list: str):
        writer.write("class {}({}):\n".format(class_name, base_name))

        # Constructor.
        writer.write("    def __init__(self, {}):\n".format(field_list))

        # Store parameters in fields.
        fields = field_list.split(", ")
        for field in fields:
            name = field.split(":")[0]
            writer.write("        self.{} = {}\n".format(name, name))

        writer.write("\n")

        # Visitor pattern.
        writer.write("    def accept(self, visitor):\n")
        writer.write("        return visitor.visit_{}_{}(self)\n".format(class_name.lower(), base_name.lower()))

        writer.write("\n")
        writer.write("\n")


    @classmethod
    def define_visitor(cls, writer, base_name: str, types: list[str]):
        writer.write("    class Visitor:\n")

        for type_ in types:
            type_name = type_.split(";")[0].strip()
            writer.write("        def visit_{}_{}(self, {}: '{}'):\n".format(type_name.lower(), base_name.lower(), base_name.lower(), type_name))
            writer.write("            raise NotImplementedError\n")
            writer.write("\n")

        writer.write("\n")



if __name__ == "__main__":
    GenerateAST.main()