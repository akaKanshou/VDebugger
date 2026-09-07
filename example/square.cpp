#include <iostream>

// inline

inline int compute_inline(int n) { return n * n + 1; }

int compute_twice_inline(int n) {
    return compute_inline(n) + compute_inline(n + 5);
}

int compute_recursive_inline(int n) {
    return compute_inline(compute_inline(n + 5));
}

// non inline

int compute(int n) { return n * n - 1; }

int compute_twice(int n) { return compute(n) + compute(n + 5); }

int compute_recursive(int n) { return compute(compute(n + 5)); }

int main() {
    int n;
    std::cin >> n;

    std::cerr << compute(n) << "\n";
    std::cerr << compute_twice(n) << "\n";
    std::cerr << compute_recursive(n) << "\n";

    std::cerr << "done\n";

    std::cerr << compute_inline(n) << "\n";
    std::cerr << compute_twice_inline(n) << "\n";
    std::cerr << compute_recursive_inline(n) << "\n";

    std::cerr << "done\n";

    return 0;
}