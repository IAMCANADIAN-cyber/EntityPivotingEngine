#ifndef EXPORTER_HPP
#define EXPORTER_HPP

#include "database.hpp"
#include <string>

class Exporter {
public:
    Exporter(Database& db);
    void export_to_dot(const std::string& output_path);

private:
    Database& db_;
};

#endif // EXPORTER_HPP
