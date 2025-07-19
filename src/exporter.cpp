#include "exporter.hpp"
#include <fstream>
#include <vector>

Exporter::Exporter(Database& db) : db_(db) {}

void Exporter::export_to_dot(const std::string& output_path) {
    std::ofstream out(output_path);
    out << "digraph G {" << std::endl;

    std::vector<std::string> domains = db_.get_domains();
    for (const auto& domain : domains) {
        out << "  \"" << domain << "\";" << std::endl;
    }

    // TODO: Add edges

    out << "}" << std::endl;
}
