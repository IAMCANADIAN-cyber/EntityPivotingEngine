#include "dns_fetcher.hpp"
#include <iostream>

static void dns_callback(void* arg, int status, int timeouts, unsigned char* abuf, int alen) {
    if (status == ARES_SUCCESS) {
        std::cout << "Successfully resolved domain" << std::endl;
    } else {
        std::cout << "Failed to resolve domain: " << ares_strerror(status) << std::endl;
    }
}

DnsFetcher::DnsFetcher() {
    int status = ares_library_init(ARES_LIB_INIT_ALL);
    if (status != ARES_SUCCESS) {
        std::cerr << "ares_library_init failed: " << ares_strerror(status) << std::endl;
    }

    status = ares_init(&channel_);
    if (status != ARES_SUCCESS) {
        std::cerr << "ares_init failed: " << ares_strerror(status) << std::endl;
    }
}

DnsFetcher::~DnsFetcher() {
    ares_destroy(channel_);
    ares_library_cleanup();
}

void DnsFetcher::resolve(const std::string& domain) {
    ares_query(channel_, domain.c_str(), C_IN, T_A, dns_callback, NULL);
}
