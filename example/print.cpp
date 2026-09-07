#include <iostream>

void print(int n) {
    while (n > 0) {
        std::cerr << n << " ";
        n--;
    }
}

void call_print(int n) {
    int m = 2 * n;
    print(m);
}

int main() {
    int n;
    std::cin >> n;

    call_print(n);

    return 0;
}