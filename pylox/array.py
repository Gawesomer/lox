from util import stringify


class LoxArray:

    def __init__(self, elements: list[object]):
        self.elements = elements

    def get(self, index: int) -> object:
        return self.elements[index]

    def set(self, index: int, value: object):
        self.elements[index] = value

    def __eq__(self, other) -> bool:
        if isinstance(other, LoxArray):
            return self.elements == other.elements
        return False

    def __len__(self) -> int:
        return len(self.elements)

    def __str__(self) -> str:
        return "[{}]".format(",".join([stringify(element) for element in self.elements]))
