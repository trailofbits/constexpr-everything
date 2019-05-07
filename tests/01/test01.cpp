#include <iostream>
#include <string>

int my_strlen(const char *in)
{
    int rv = 0;
    while (*in++)
        rv++;

    return rv;
}

std::string get_input()
{
    std::string in;
    std::getline(std::cin, in);

    return in;
}

int main(int argc, char **argv)
{
    int first = my_strlen(argv[0]);
    int second = my_strlen(get_input().c_str());
    int third = my_strlen("aaaaaaaaaaaaaaaaaaaaaa");

    std::cout << "first: " << first << "\n";
    std::cout << "second: " << second << "\n";
    std::cout << "third: " << third << "\n";

    return 0;
}
