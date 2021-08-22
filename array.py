from exception import IndexException


class LoxArray:

    def __init__(self, elements: list[object]):
        self.elements = elements

    def get(self, index: float) -> object:
        return self.elements[self.clean_index(index)]

    def set(self, index: float, value: object):
        self.elements[self.clean_index(index)] = value

    def clean_index(self, index: float):
        index = int(index)
        if 0 <= index < len(self.elements):
            return index
        raise IndexException("Invalid array index.")

    def __str__(self) -> str:
        return "[{}]".format(",".join([str(element) for element in self.elements]))
