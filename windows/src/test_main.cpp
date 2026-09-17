#include "accounting.hpp"
#include <iostream>

int main() {
    std::cout << "Running Ratio Accounting Self-Tests (Windows Port)...\n";
    bool ok = Classifier::runSelfTest();
    if (ok) {
        std::cout << "\n=========================================\n";
        std::cout << "SUCCESS: All accounting and classification tests passed!\n";
        std::cout << "=========================================\n";
        return 0;
    } else {
        std::cerr << "\nFAILURE: One or more self-tests failed!\n";
        return 1;
    }
}
