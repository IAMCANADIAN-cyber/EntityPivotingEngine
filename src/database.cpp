#include "database.hpp"
#include <sqlite3.h>
#include <iostream>

Database::Database(const std::string& db_path) : db_path_(db_path) {}

void Database::initialize_schema() {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    const char* sql = R"SQL(
        CREATE TABLE domain (
          id INTEGER PRIMARY KEY,
          fqdn TEXT UNIQUE,
          first_seen_ts INTEGER,
          last_seen_ts INTEGER
        );

        CREATE TABLE ip_address (
          id INTEGER PRIMARY KEY,
          ip TEXT UNIQUE,
          asn INTEGER,
          as_org TEXT,
          geo_cc TEXT
        );

        CREATE TABLE dns_record (
          id INTEGER PRIMARY KEY,
          domain_id INTEGER,
          type TEXT,
          value TEXT,
          ttl INTEGER,
          first_seen_ts INTEGER,
          last_seen_ts INTEGER,
          FOREIGN KEY(domain_id) REFERENCES domain(id)
        );

        CREATE INDEX idx_dns_domain_type ON dns_record(domain_id, type);

        CREATE TABLE whois_snapshot (
          id INTEGER PRIMARY KEY,
          domain_id INTEGER,
          snapshot_ts INTEGER,
          registrar TEXT,
          registrant_name TEXT,
          registrant_org TEXT,
          registrant_email TEXT,
          name_servers TEXT,
          raw TEXT,
          hash TEXT,
          FOREIGN KEY(domain_id) REFERENCES domain(id)
        );

        CREATE TABLE tls_certificate (
          id INTEGER PRIMARY KEY,
          fingerprint_sha256 TEXT UNIQUE,
          subject_cn TEXT,
          san TEXT,
          issuer TEXT,
          not_before INTEGER,
          not_after INTEGER,
          serial TEXT
        );

        CREATE TABLE tls_binding (
          id INTEGER PRIMARY KEY,
          domain_id INTEGER,
          cert_id INTEGER,
          fetch_ts INTEGER,
          FOREIGN KEY(domain_id) REFERENCES domain(id),
          FOREIGN KEY(cert_id) REFERENCES tls_certificate(id)
        );

        CREATE TABLE favicon_hash (
          id INTEGER PRIMARY KEY,
          domain_id INTEGER,
          hash_algo TEXT,
          hash_value TEXT,
          bytes INTEGER,
          fetch_ts INTEGER
        );

        CREATE TABLE http_fingerprint (
          id INTEGER PRIMARY KEY,
          domain_id INTEGER,
          fetch_ts INTEGER,
          server_header TEXT,
          set_cookie_patterns TEXT,
          analytics_ids TEXT,
          cms_tags TEXT,
          raw_headers TEXT
        );

        CREATE TABLE content_simhash (
          id INTEGER PRIMARY KEY,
          domain_id INTEGER,
          fetch_ts INTEGER,
          simhash64 TEXT,
          text_length INTEGER
        );

        CREATE TABLE social_handle (
          id INTEGER PRIMARY KEY,
          domain_id INTEGER,
          platform TEXT,
          handle TEXT,
          first_seen_ts INTEGER,
          last_seen_ts INTEGER
        );

        CREATE TABLE entity_node (
          id INTEGER PRIMARY KEY,
          type TEXT,
          key TEXT,
          first_seen_ts INTEGER,
          last_seen_ts INTEGER
        );

        CREATE UNIQUE INDEX ux_entity_type_key ON entity_node(type, key);

        CREATE TABLE entity_edge (
          id INTEGER PRIMARY KEY,
          src_id INTEGER,
          dst_id INTEGER,
          relation TEXT,
          weight REAL,
          first_seen_ts INTEGER,
          last_seen_ts INTEGER,
          FOREIGN KEY(src_id) REFERENCES entity_node(id),
          FOREIGN KEY(dst_id) REFERENCES entity_node(id)
        );

        CREATE INDEX idx_edge_src ON entity_edge(src_id);
        CREATE INDEX idx_edge_dst ON entity_edge(dst_id);
    )SQL";

    char* zErrMsg = 0;
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Database schema initialized successfully." << std::endl;
    }

    sqlite3_close(db);
}

void Database::add_seeds(const std::vector<std::string>& seeds) {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    const char* sql = "INSERT INTO domain (fqdn, first_seen_ts, last_seen_ts) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION", 0, 0, 0);

    for (const auto& seed : seeds) {
        long long current_time = time(0);
        sqlite3_bind_text(stmt, 1, seed.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, current_time);
        sqlite3_bind_int64(stmt, 3, current_time);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_exec(db, "COMMIT TRANSACTION", 0, 0, 0);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

std::vector<std::string> Database::get_domains() {
    sqlite3* db;
    int rc = sqlite3_open(db_path_.c_str(), &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return {};
    }

    std::vector<std::string> domains;
    const char* sql = "SELECT fqdn FROM domain";
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return {};
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        domains.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return domains;
}
