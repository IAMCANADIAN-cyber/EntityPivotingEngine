# EntityPivotingEngine
Written in C++;

Cross‑Platform Domain / Entity Pivoting Engine What: Local CLI/GUI tool that ingests lists of domains, social handles, WHOIS data, and auto-builds a relationship graph (export .graphml).
Features: WHOIS parsing & delta tracking, SSL certificate reuse clustering, favicon hash correlation, DNS history snapshots, simple content fingerprinting (simhash).

Stack: cpr or libcurl (HTTP), nlohmann::json, sqlite3, Botan/OpenSSL, simhash implementation, GraphViz DOT export.

Hard Bits: Efficient async HTTP (Boost.Asio), rate-limiting, caching, normalized entity dedupe.

Stretch: Integrate lightweight C++ Bloom filters to quickly test “seen content chunk” for laundering patterns.

ChatGPT said: Below is a comprehensive architecture & implementation plan for the Cross‑Platform Domain / Entity Pivoting Engine. It’s structured so you can start with a lean CLI MVP and evolve toward a richer GUI + advanced correlations. Skim the outline first, then dive into any section you want more detail on.

High-Level Goals Mission: Given a seed list (domains, URLs, org names, social handles, IPs), automatically discover & correlate technical & contextual artifacts to reveal relationships (common ownership / infrastructure reuse / content laundering), and export a navigable graph (.graphml / DOT / JSON).
Key Correlation Axes (“pivots”):

WHOIS / RDAP: registrant emails, org names, name servers.

DNS / Hosting: A / AAAA / NS / MX / TXT (SPF) / CNAME, ASN, reverse PTR.

TLS / Certificates: Subject CN/SAN sets, certificate serial, issuer, validity, reuse across domains.

Favicon: Perceptual hash or MD5 of raw bytes (common across clones).

HTTP Fingerprints: Server header, set-cookie patterns, analytics IDs (GA, GTM, Pixel), CMS meta tags.

Content Fingerprint: SimHash of normalized text / script snippet families.

Social / External IDs: Extracted from page (Twitter/X handle, YouTube channel ID, etc).

Timing / Delta: Changes in WHOIS (registrant changed), DNS hosting moved, certificate rotation.

System Architecture Overview 2.1 Layered Components Layer Responsibility Key Tech CLI / (later) GUI Commands, progress reporting, graph visual filter & export CLI: argparse; GUI: ImGui/Qt Orchestrator Plan tasks, rate-limit pools, dependency scheduling Custom Task Scheduler (Boost.Asio) Acquisition Modules Network/HTTP, WHOIS/RDAP, DNS queries, TLS fetch cpr/libcurl, Boost.Asio, OpenSSL Normalization & Parsing Clean & canonicalize raw data ICU, custom parsers Correlation Engine Entity resolution, edge inference SQLite + in-memory maps Persistence Layer Store raw & derived artifacts SQLite (WAL), optional column tables Fingerprinting Favicon hash, SimHash, TLS cert digest OpenSSL, custom SimHash Bloom / Cache Layer “Seen” content blocks, fetch dedupe Custom Bloom, LRU caches Export Layer GraphML, DOT, JSON line deltas Custom serializers Metrics & Logging Timing, error rates, throughput spdlog + Prometheus text endpoint (optional)
2.2 Data Flow rust Copy Edit Seed Input -> Task Planner -> (Parallel Fetch Pipelines) └-> Raw Artifacts (WHOIS / DNS / HTTP / TLS / Content) -> Normalizers -> Artifact Tables -> Fingerprinting / Hashing -> Correlation Rules -> Graph Update -> Export / UI Visualization 2.3 Concurrency Model Boost.Asio I/O Context: N worker threads (configurable).

Task Graph: Each seed spawns subtasks (DNS, WHOIS, HTTP HEAD, TLS cert, HTML fetch). Completion triggers correlation tasks.

Rate Limiting: Token buckets per source type:

WHOIS: strict (e.g. 1 qps, burst 2).

HTTP: moderate (e.g. 10 concurrent).

DNS: local resolver (fast) but still cap to avoid NS flooding.

Backpressure: Global “in-flight task” counter; if above threshold, queue.

Data Model (SQLite Schema) 3.1 Core Tables sql Copy Edit -- Seeds and resolved domains CREATE TABLE domain ( id INTEGER PRIMARY KEY, fqdn TEXT UNIQUE, first_seen_ts INTEGER, last_seen_ts INTEGER );
CREATE TABLE ip_address ( id INTEGER PRIMARY KEY, ip TEXT UNIQUE, asn INTEGER, as_org TEXT, geo_cc TEXT );

CREATE TABLE dns_record ( id INTEGER PRIMARY KEY, domain_id INTEGER, type TEXT, -- A, AAAA, NS, MX, TXT, CNAME value TEXT, ttl INTEGER, first_seen_ts INTEGER, last_seen_ts INTEGER, FOREIGN KEY(domain_id) REFERENCES domain(id) );

CREATE INDEX idx_dns_domain_type ON dns_record(domain_id, type);

CREATE TABLE whois_snapshot ( id INTEGER PRIMARY KEY, domain_id INTEGER, snapshot_ts INTEGER, registrar TEXT, registrant_name TEXT, registrant_org TEXT, registrant_email TEXT, name_servers TEXT, -- JSON array raw TEXT, hash TEXT, -- SHA256 of raw FOREIGN KEY(domain_id) REFERENCES domain(id) );

CREATE TABLE tls_certificate ( id INTEGER PRIMARY KEY, fingerprint_sha256 TEXT UNIQUE, subject_cn TEXT, san JSON, -- JSON array issuer TEXT, not_before INTEGER, not_after INTEGER, serial TEXT );

CREATE TABLE tls_binding ( id INTEGER PRIMARY KEY, domain_id INTEGER, cert_id INTEGER, fetch_ts INTEGER, FOREIGN KEY(domain_id) REFERENCES domain(id), FOREIGN KEY(cert_id) REFERENCES tls_certificate(id) );

CREATE TABLE favicon_hash ( id INTEGER PRIMARY KEY, domain_id INTEGER, hash_algo TEXT, -- md5, mmh3, phash hash_value TEXT, bytes INTEGER, fetch_ts INTEGER );

CREATE TABLE http_fingerprint ( id INTEGER PRIMARY KEY, domain_id INTEGER, fetch_ts INTEGER, server_header TEXT, set_cookie_patterns TEXT, -- JSON array of regex categories analytics_ids TEXT, -- JSON (GA IDs, FB Pixel) cms_tags TEXT, -- JSON (e.g. "wordpress", "wix") raw_headers TEXT );

CREATE TABLE content_simhash ( id INTEGER PRIMARY KEY, domain_id INTEGER, fetch_ts INTEGER, simhash64 TEXT, text_length INTEGER );

CREATE TABLE social_handle ( id INTEGER PRIMARY KEY, domain_id INTEGER, platform TEXT, -- twitter, youtube, telegram handle TEXT, first_seen_ts INTEGER, last_seen_ts INTEGER );

CREATE TABLE entity_node ( id INTEGER PRIMARY KEY, type TEXT, -- domain, ip, cert, registrant_email, org, favicon, simhash, analytics_id, social key TEXT, -- canonical key value first_seen_ts INTEGER, last_seen_ts INTEGER );

CREATE UNIQUE INDEX ux_entity_type_key ON entity_node(type, key);

CREATE TABLE entity_edge ( id INTEGER PRIMARY KEY, src_id INTEGER, dst_id INTEGER, relation TEXT, -- e.g. "RESOLVES_TO", "USES_CERT", "SHARES_FAVICON", "REG_EMAIL", "CONTENT_NEAR_DUP" weight REAL, first_seen_ts INTEGER, last_seen_ts INTEGER, FOREIGN KEY(src_id) REFERENCES entity_node(id), FOREIGN KEY(dst_id) REFERENCES entity_node(id) );

CREATE INDEX idx_edge_src ON entity_edge(src_id); CREATE INDEX idx_edge_dst ON entity_edge(dst_id); 3.2 Entity Canonicalization Rules Type Canonical Key Strategy domain Lowercase punycode FQDN ip Original string cert SHA256 fingerprint registrant_email Lowercase trimmed org Trim punctuation, collapse whitespace, casefold favicon hash_algo:hash_value simhash 64-bit hex lowercase analytics_id e.g. UA-XXXX, G-XXXX, fb:XXXXXXXX social

3.3 Edge Weighting Heuristic (Initial) RESOLVES_TO (domain→ip): weight = 0.5 / (#domains on IP capped) to downweight shared hosting noise.

USES_CERT (domain→cert): 1.0

CERT_SAN (cert→domain): 0.8

REG_EMAIL (domain→email): 1.2

REG_ORG (domain→org): 0.9

SHARES_FAVICON (domain↔domain): 0.7 (bidirectional)

CONTENT_NEAR_DUP (domain↔domain): dynamic = 0.6 * similarity (Jaccard of shingles)

SHARES_ANALYTICS_ID: 1.1

SHARES_SOCIAL_HANDLE: 1.0

ASN_COHOST (ip→asn org): 0.3

Allow later tuning via config file.

Acquisition & Parsing Modules Module Input Output Notes DNSFetcher domain dns_record rows Use asynchronous resolver (c-ares or native Asio). WHOISFetcher domain whois_snapshot RDAP first, fallback WHOIS. Hash to detect deltas. TLSFetcher domain tls_certificate + tls_binding TCP 443 handshake; parse cert chain. HTTPHeadFetcher domain http_fingerprint (headers) HEAD/GET with redirect follow (limit). FaviconFetcher domain favicon_hash GET /favicon.ico or parse link rel=icon. HTMLFetcher domain content_simhash + extracted social & analytics GET root, extract & normalize text. GeoASNResolver ip ip_address enrichment Use IP2ASN DB / MaxMind ASN.
Normalization Steps (HTML):

Strip scripts/styles.

Lowercase.

Remove boilerplate tokens (stopwords + numeric noise).

Tokenize into shingles (k=4).

Compute SimHash (64-bit).

Extract regex hits (analytics IDs, social patterns).

Correlation Logic Trigger Points:
Insertion of new artifact triggers:

Entity node ensure/lookup.

Edge creations referencing existing nodes with same canonical key.

For SimHash: Hamming distance <= 3 → create CONTENT_NEAR_DUP edges.

Implement multi-index buckets (e.g., 4-part partition technique for near-duplicate search).

Favicon Reuse: If identical hash previously seen > once & bytes size matches, add SHARES_FAVICON edges.

Registrar Delta: When new whois_snapshot hash differs; compute diff to highlight new registrant_email/org changes (store diff JSON in a side table if desired).

SimHash Implementation Sketch cpp Copy Edit uint64_t compute_simhash(const std::vectorstd::string& shingles) { std::array<int, 64> acc{}; for (const auto& sh : shingles) { uint64_t h = cityhash64(sh.data(), sh.size()); for (int i=0;i<64;++i) { acc[i] += (h & (1ULL<<i)) ? 1 : -1; } } uint64_t result = 0; for (int i=0;i<64;++i) if (acc[i] > 0) result |= (1ULL<<i); return result; } Near-Dup Index: Store four 16-bit chunks; for a lookup, query candidate sets where any single chunk matches.

Bloom Filter Stretch Use Cases:

Fast membership test: “Have we already processed this normalized content block?”

Memory target: e.g. 10 million entries, false positive ~1% → m ≈ 9.6 bits/entry.

Config:

yaml Copy Edit bloom: expected_items: 10000000 false_positive_rate: 0.01 hash_functions: 7 Integration: Before scheduling HTMLFetcher, hash (domain, last_content_simhash); skip fetch if unchanged in LRU + Bloom to reduce redundant downloads.

Rate Limiting & Caching Resource Strategy Storage WHOIS/RDAP Token bucket (1/sec) + persistent cache (24h TTL) keyed by domain hash SQLite cache table HTTP DNS pre-check, exponential backoff on 429/5xx In-memory LRU (domain→etag/last-modified) Favicon Only refetch if HTTPHead ETag changed Same LRU TLS Cache per domain until cert not_before changes SQLite DNS Respect TTL caching, fallback min TTL (300s) In-memory TTL map
cache table example:

sql Copy Edit CREATE TABLE cache ( key TEXT PRIMARY KEY, value BLOB, fetched_ts INTEGER, ttl_seconds INTEGER ); 9. CLI Command Plan Command Purpose Example pivot init db.sqlite Initialize schema pivot add-seeds file.txt Insert domains/handles pivot add-seeds seeds.txt pivot run --max-concurrency 32 --duration 600s Run collectors pivot snapshot Force all pending tasks flush pivot export --format graphml --out graph.graphml Export pivot query --type domain --key example.com Inspect entity pivot diff-whois example.com Show whois delta history pivot stats Display metrics

GraphML Export Schema (Simplified) Node Attributes:
id (integer)

type (string)

label (string = key or fqdn)

first_seen (epoch)

last_seen (epoch)

Edge Attributes:

id

source

target

relation

weight

first_seen

last_seen

Example snippet:

xml Copy Edit 11. Entity Resolution (Dedupe) Algorithm Lookup / Insert Pattern:

Build per-type unordered_map cache (LRU size-limited) mirroring entity_node.

On new artifact:

Compute canonical key.

Hash map hit → update last_seen_ts.

Miss → insert row; store ID in map.

String Normalization:

Emails: lower(trim).

Org: Unicode NFKC → remove trailing punctuation → collapse whitespace → lower.

Fuzzy Org Merge (Phase 2):

Maintain trigram index of org strings.

If Jaro-Winkler >= 0.95, unify by adding EDGES ORG_ALIAS rather than merging rows (keeps provenance).

Performance:

For 100k domains: aim < 5 ms per new artifact insert with prepared statements + transactions batched.

Task Scheduling & Dependency Example Task Depends On Output Triggers DNS Seed domain IP enrichment, TLS, HTTP WHOIS Seed domain Registrant edges TLS DNS (A record success) CERT entity, SAN domain seeds HTTP HEAD DNS Decide if need GET/HTML Favicon HTTP HEAD (200 + content-type html or icon link) FAVICON edges HTML GET HTTP HEAD SimHash + analytics + social seeds
Dynamic Seeding: New domains discovered in:

Certificate SANs

HTML links (same site?). Optional flag: --expand-scope with domain regex allowlist.

Configuration File (YAML Example) yaml Copy Edit fetch: concurrency: 32 timeouts: dns_ms: 3000 http_ms: 5000 whois_ms: 8000 retries: 2
rate_limits: whois_qps: 1 http_max_parallel: 16

simhash: shingle_size: 4 hamming_threshold: 3 min_text_len: 500

dedupe: favicon_min_bytes: 200 content_min_len: 800

export: max_edge_weight: 2.0 14. Performance Targets (MVP) Metric Target Domains processed/hour (with WHOIS) 2–3k (WHOIS is bottleneck) Domains processed/hour (cache hits) 10–15k Avg HTTP fetch latency (p95) < 1.2s Memory footprint (50k domains) < 1.5 GB Graph export time (50k nodes, 300k edges) < 10s

Optimizations later: batch DB inserts (BEGIN…COMMIT every N operations), memory-mapped read-only analytics after run.

Logging & Metrics Levels: TRACE (dev), INFO (progress), WARN, ERROR.
Counters: fetch_success_total{type}, fetch_fail_total{type}, edge_count{relation}, simhash_compare_ops.

Histograms: fetch_latency_seconds{type}, task_queue_wait_seconds.

Expose optional HTTP endpoint localhost:9090/metrics.

Testing Strategy Test Tier Focus Example Unit SimHash correctness, canonicalization Hamming distance test set Unit WHOIS parser extraction Fixture raw WHOIS texts (registrar variants) Integration Full pipeline with mock HTTP/DNS servers Docker compose harness Performance Batch 10k synthetic domains Measure throughput Regression Graph export stability Golden GraphML snapshot diff Property-Based Entity dedupe idempotency Random capitalization/original forms Security Malformed input robustness Fuzz HTML & WHOIS parser with libFuzzer
Fixtures: Provide synthetic domain set with intentional shared favicon, shared analytics ID, cert reuse.

Risk Register & Mitigations Risk Impact Mitigation WHOIS rate limiting / blocking Slow pipeline Cache + RDAP where possible; allow user-provided offline WHOIS dumps. Shared hosting noise (false links) Graph clutter Weighting heuristics; filter IPs with > N domains. False positives from favicon reuse on CDN Spurious edges Mark CDN-detected (Server header, known IP ranges) and downweight. SimHash collisions Incorrect content linking Hamming threshold conservative (<=3) + min text length gate. Memory growth OOM Bounded caches; periodic vacuum; optional prune old snapshots. Multi-thread DB contention Latency Single writer queue (command pattern) or WAL + prepared statements batching. Legal/ethical misuse Reputational Clear disclaimer; user-supplied seeds only; rate-limit defaults.

Incremental Milestones Milestone Scope Output M1 Core DB + CLI Skeleton Schema, pivot init, seed loading Seeds stored M2 DNS + IP Enrichment Async DNS, ip_address table, IP nodes/edges Domain↔IP graph M3 HTTP Headers + Favicon GET/HEAD, favicon hash edges Fingerprint edges M4 TLS Certificates TLS handshake, SAN expansion (optional) CERT nodes & edges M5 WHOIS / RDAP WHOIS fetch + parsing + diff Registrant edges M6 HTML + SimHash + Analytics Content fetch, simhash, analytics ID edges Content-based links M7 Correlation Refinements Edge weights, dedupe improvements Cleaner graph M8 Export Layer GraphML/DOT/JSON exports Visualization-ready graph M9 Optimization + Caching Rate limits, Bloom, LRU Performance targets hit M10 GUI (Optional) ImGui interactive filters, timeline Visual exploration

Pseudocode (Pipeline Highlights) cpp Copy Edit struct Task { enum Type { DNS, WHOIS, TLS, HTTP_HEAD, HTML, FAVICON } type; std::string domain; int retry = 0; };

class Scheduler { void submit(Task t); void workerLoop(); // pulls tasks, executes, enqueues follow-ons };

void handleDnsResult(const DnsResult& r) { upsertDomain(r.domain); for (auto& a : r.a_records) { upsertIP(a.ip); linkDomainIp(r.domain, a.ip); enqueue(Task{Task::TLS, r.domain}); enqueue(Task{Task::HTTP_HEAD, r.domain}); } }

void handleHttpHead(const HttpHeadResult& r) { upsertHttpFingerprint(...); if (shouldFetchHTML(r)) enqueue(Task{Task::HTML, r.domain}); if (r.hasIcon) enqueue(Task{Task::FAVICON, r.domain}); }

void handleHtml(const HtmlResult& r) { auto simhash = computeSimhash(r.tokens); auto nearDups = simhashIndex.query(simhash); for (auto& d : nearDups) linkContentNearDup(r.domain, d); for (auto& analyticsId : r.analyticsIds) linkAnalytics(r.domain, analyticsId); } 20. GUI (Phase 2) Concept Panels:

Graph Explorer: Filter nodes by type, degree, time window.

Timeline: Changes (WHOIS, DNS moves) as event list.

Entity Detail: Raw artifact previews.

Search: Regex or exact by key.

Pivot Actions: Right-click an entity → spawn new tasks (if expansion enabled).

Extensibility (Plugins) Plugin ABI: Shared library exports extern "C" ICorrelationPlugin* create_plugin();
Interface: Methods on_new_artifact(const Artifact&) → return vector

Use Case: Adding new correlation (e.g., shared Google AdSense ID, language detection).

Quality Gates Before Each Milestone Gate Criteria G1 Unit tests pass (<2s) G2 Static analysis (clang-tidy) zero critical issues G3 Memory sanitizer run on synthetic set leak-free G4 Performance smoke: 1k domains < 5 min M6 G5 Export validates against GraphML XSD

Example Configurable Edge Filtering (Noise Control) yaml Copy Edit filters: max_domains_per_ip: 30 ignore_tlds: ["tk","ml"] # Optional min_favicon_bytes: 128 min_content_length: 600 skip_analytics_ids: ["UA-XXXXX-Y"] # Known shared test IDs

Documentation Structure (Proposed) README (Purpose, quickstart)

ARCHITECTURE.md (This plan condensed)

SCHEMA.md

DEVELOPING.md (Build, tooling, code style)

CONFIGURATION.md

EXTENDING.md (Plugin API example)

LEGAL_ETHICS.md (Proper usage guidelines)

First Actionable Steps (Concrete “Do Next”) Initialize repo with CMake, add dependencies (nlohmann::json, spdlog).
Implement Schema migration tool (single migrations.cpp).

Write Seed loader & pivot init, pivot add-seeds.

Add DNSFetcher with asynchronous resolution; confirm domain→IP edges appear.

Add Export (minimal) to DOT just for early validation.

From there move to M3 (HTTP) which enables visible correlations quickly.

Optional Early MVP Cut (If Time-Constrained) Skip WHOIS & SimHash initially:
M2 + M3 + M4 (DNS + HTTP headers + TLS) already produce meaningful infra reuse relationships.

Add Favicon hashing early (fast high-signal pivot).

Later append WHOIS (legal complexity & rate-limits) and content fingerprints.
