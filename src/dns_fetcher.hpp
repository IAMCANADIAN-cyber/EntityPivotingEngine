#ifndef DNS_FETCHER_HPP
#define DNS_FETCHER_HPP

#include <string>
#include <vector>

class DnsFetcher {
public:
    DnsFetcher();
    ~DnsFetcher();

    void resolve(const std::string& domain);

private:
    // c-ares channel
    void* channel_;
};

#endif // DNS_FETCHER_HPP
