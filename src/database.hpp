#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <vector>

class Database {
public:
    Database(const std::string& db_path);
    void initialize_schema();
    void add_seeds(const std::vector<std::string>& seeds);
    std::vector<std::string> get_domains();

private:
    std::string db_path_;
};

#endif // DATABASE_HPP
