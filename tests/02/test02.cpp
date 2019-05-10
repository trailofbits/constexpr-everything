#include <iostream>

class X {
    private:
        int num;
    public:
        //Default constructor
        X() : num(0) {
            std::cout << "\tDefault Constructor\n";
        }

        //Overloaded constructor
        X(const int& val) : num(val) {
        }

        //Copy constructor
        X(const X& lVal)
            : num(lVal.num) {
                std::cout << "\tCopy constructor\n";
        }

        //Copy assignment constructor
        X& operator=(const X& lVal) {
            this->num = lVal.num;
            std::cout << "\tCopy Assignment Operator\n";
            return *this;
        }

        int getNum() const { return num; }

};

X doSomething() {
    X x(100);

    return x;
}

int main() {
    std::cout << "x1:\n";
    const X x1 = doSomething();

    std::cout << "x2:\n";
    X x2;
    x2 = doSomething();

    const X x3(400);

    std::cout << x3.getNum() << "\n";

    const X x4(x3);
    const X x5(x3);

    std::cout << x4.getNum() << "\n";
}
