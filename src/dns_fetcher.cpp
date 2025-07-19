#include "dns_fetcher.hpp"
#include <iostream>

// c-ares headers would be included here, e.g.,
// #include <ares.h>

DnsFetcher::DnsFetcher() {
    // ares_library_init would be called here
    std::cout << "DnsFetcher initialized (c-ares not implemented)" << std::endl;
}

DnsFetcher::~DnsFetcher() {
    // ares_library_cleanup would be called here
    std::cout << "DnsFetcher destroyed (c-ares not implemented)" << std::endl;
}

void DnsFetcher::resolve(const std::string& domain) {
    std::cout << "Resolving " << domain << " (c-ares not implemented)" << std::endl;
    // ares_gethostbyname would be called here
}
