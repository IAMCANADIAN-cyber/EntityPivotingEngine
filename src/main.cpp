#include "database.hpp"
#include "dns_fetcher.hpp"
#include "exporter.hpp"
#include "argparse.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("pivot");

    argparse::ArgumentParser init_command("init");
    init_command.add_argument("db_path")
        .help("path to the database file");

    argparse::ArgumentParser add_seeds_command("add-seeds");
    add_seeds_command.add_argument("db_path")
        .help("path to the database file");
    add_seeds_command.add_argument("seeds_file")
        .help("path to the file containing seeds");

    argparse::ArgumentParser run_command("run");
    run_command.add_argument("db_path")
        .help("path to the database file");

    argparse::ArgumentParser export_command("export");
    export_command.add_argument("db_path")
        .help("path to the database file");
    export_command.add_argument("--format")
        .default_value("dot")
        .help("output format (dot)");
    export_command.add_argument("--out")
        .required()
        .help("output file path");

    program.add_subparser(init_command);
    program.add_subparser(add_seeds_command);
    program.add_subparser(run_command);
    program.add_subparser(export_command);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    if (program.is_subcommand_used("init")) {
        Database db(init_command.get<std.string>("db_path"));
        db.initialize_schema();
    } else if (program.is_subcommand_used("add-seeds")) {
        Database db(add_seeds_command.get<std::string>("db_path"));
        std::ifstream seeds_file(add_seeds_command.get<std::string>("seeds_file"));
        if (!seeds_file) {
            std::cerr << "Could not open seeds file" << std::endl;
            return 1;
        }
        std::vector<std::string> seeds;
        std::string line;
        while (std::getline(seeds_file, line)) {
            seeds.push_back(line);
        }
        db.add_seeds(seeds);
        std::cout << "Added " << seeds.size() << " seeds to the database." << std::endl;
    } else if (program.is_subcommand_used("run")) {
        Database db(run_command.get<std::string>("db_path"));
        DnsFetcher dns_fetcher;
        std::vector<std::string> domains = db.get_domains();
        for (const auto& domain : domains) {
            dns_fetcher.resolve(domain);
        }
    } else if (program.is_subcommand_used("export")) {
        Database db(export_command.get<std::string>("db_path"));
        Exporter exporter(db);
        exporter.export_to_dot(export_command.get<std::string>("--out"));
        std::cout << "Exported graph to " << export_command.get<std::string>("--out") << std::endl;
    }

    return 0;
}
