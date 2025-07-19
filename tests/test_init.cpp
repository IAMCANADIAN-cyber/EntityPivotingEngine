#include "database.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

int main() {
    const std::string db_path = "test.db";
    Database db(db_path);
    db.initialize_schema();

    // Check that the database file was created
    std::ifstream f(db_path.c_str());
    assert(f.good());

    // Check that the tables were created
    sqlite3* test_db;
    int rc = sqlite3_open(db_path.c_str(), &test_db);
    assert(rc == SQLITE_OK);

    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(test_db, "SELECT name FROM sqlite_master WHERE type='table'", -1, &stmt, 0);
    assert(rc == SQLITE_OK);

    int table_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        table_count++;
    }
    assert(table_count > 10); // Check for a reasonable number of tables

    sqlite3_finalize(stmt);
    sqlite3_close(test_db);

    std::cout << "Test passed!" << std::endl;

    return 0;
}
