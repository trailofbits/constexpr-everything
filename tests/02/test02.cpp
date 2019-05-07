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
                std::cout << "\tCopy Constructor\n";
        }

        //Copy assignment constructor
        X& operator=(const X& lVal) {
            this->num = lVal.num;
            std::cout << "\tCopy Assignment Operator\n";
            return *this;
        }

};

X doSomething() {
    X x(100);

    return x;
}

int main() {
    std::cout << "x1:\n";
    X x1 = doSomething();

    std::cout << "x2:\n";
    X x2;
    x2 = doSomething();

    X x3(400);
}
