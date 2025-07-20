#ifndef DNS_FETCHER_HPP
#define DNS_FETCHER_HPP

#include <string>
#include <vector>
#include "ares.h"

class DnsFetcher {
public:
    DnsFetcher();
    ~DnsFetcher();

    void resolve(const std::string& domain);

private:
    ares_channel channel_;
};

#endif // DNS_FETCHER_HPP
