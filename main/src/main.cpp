#ifdef _WIN32
#include <windows.h>
#endif

#include "cli/runner.hpp"

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    return noldoj::cli::run(argc, argv);
}
