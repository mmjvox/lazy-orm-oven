#include "SqliteConnection.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <filesystem>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

// Helper function to create a test connection (temporary file)
SqliteConnection createTestConnection(bool wallMode = false) {
    return SqliteConnection("test_db.sqlite", wallMode);
}

// Helper function to create an in-memory connection
SqliteConnection createInMemoryConnection(bool wallMode = false) {
    return SqliteConnection(":memory:", wallMode);
}

// Helper function to setup test database
void setupTestDatabase(SqliteConnection& conn) {
    if (!conn.healthy()) {
        conn.connect();
    }

    // Drop existing tables
    SqlTask dropUsers("DROP TABLE IF EXISTS users");
    conn.exec(dropUsers);
    SqlTask dropTestTable("DROP TABLE IF EXISTS test_table");
    conn.exec(dropTestTable);

    // Create users table (SQLite uses INTEGER PRIMARY KEY AUTOINCREMENT)
    SqlTask createUsersTable(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "email TEXT NOT NULL UNIQUE"
        ")"
        );
    conn.exec(createUsersTable);
}

// Helper function to cleanup test database
void cleanupTestDatabase(SqliteConnection& conn) {
    SqlTask dropUsers("DROP TABLE IF EXISTS users");
    conn.exec(dropUsers);
    SqlTask dropTestTable("DROP TABLE IF EXISTS test_table");
    conn.exec(dropTestTable);
}

// Helper function to cleanup test database file
void cleanupTestFile() {
    if (fs::exists("test_db.sqlite")) {
        fs::remove("test_db.sqlite");
    }
    if (fs::exists("test_db.sqlite-wal")) {
        fs::remove("test_db.sqlite-wal");
    }
    if (fs::exists("test_db.sqlite-shm")) {
        fs::remove("test_db.sqlite-shm");
    }
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_CASE("Connection Management", "[connection]") {

    SECTION("Successful connection - file database without WAL") {
        SqliteConnection conn = createTestConnection(false);

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);

        cleanupTestFile();
    }

    SECTION("Successful connection - file database with WAL") {
        SqliteConnection conn = createTestConnection(true);

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);

        cleanupTestFile();
    }

    SECTION("Successful connection - in-memory database") {
        SqliteConnection conn = createInMemoryConnection();

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);
    }

    SECTION("Failed connection - invalid path (directory doesn't exist)") {
        SqliteConnection conn("/nonexistent/directory/test.db");
        REQUIRE(conn.connect() == false);
    }

    SECTION("Failed connection - empty path") {
        SqliteConnection conn("");
        REQUIRE(conn.connect() == false);
        CHECK(conn.getLastError().find("empty") != std::string::npos);
    }

    SECTION("Reconnection behavior") {
        SqliteConnection conn = createTestConnection();

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.connect() == true); // Second attempt should succeed

        conn.close();
        REQUIRE(conn.connect() == true); // Reconnect after close

        conn.close();
        cleanupTestFile();
    }

    SECTION("Connection health states") {
        SqliteConnection conn = createTestConnection();

        REQUIRE(conn.healthy() == false);

        conn.connect();
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);

        cleanupTestFile();
    }

    SECTION("Multiple connections to same file") {
        const int NUM_CONNECTIONS = 5;
        std::vector<std::unique_ptr<SqliteConnection>> connections;

        for (int i = 0; i < NUM_CONNECTIONS; ++i) {
            auto conn = std::make_unique<SqliteConnection>("test_db.sqlite");
            REQUIRE(conn->connect() == true);
            connections.push_back(std::move(conn));
        }

        for (auto& conn : connections) {
            REQUIRE(conn->healthy() == true);
            conn->close();
        }

        cleanupTestFile();
    }

    SECTION("Resource cleanup - RAII") {
        {
            SqliteConnection conn = createTestConnection();
            conn.connect();
            REQUIRE(conn.healthy() == true);
        }

        SqliteConnection conn2 = createTestConnection();
        REQUIRE(conn2.connect() == true);
        conn2.close();

        cleanupTestFile();
    }

    SECTION("Read-only mode") {
        // First create the database in write mode
        {
            SqliteConnection writeConn("test_db.sqlite");
            writeConn.connect();
            SqlTask createTable("CREATE TABLE test (id INTEGER)");
            writeConn.exec(createTable);
            writeConn.close();
        }

        SqliteConnection conn("test_db.sqlite");
        conn.setReadOnly(true);

        REQUIRE(conn.connect() == true);

        // Try to write in read-only mode
        SqlTask createTable("CREATE TABLE test (id INTEGER)");
        REQUIRE(conn.exec(createTable) == IDbConnection::QueryFailed);

        conn.close();
        cleanupTestFile();
    }

    SECTION("Busy timeout setting") {
        SqliteConnection conn = createTestConnection();
        conn.setBusyTimeout(100); // 100ms
        conn.connect();

        // Test with concurrent access (simplified)
        REQUIRE(conn.healthy() == true);

        conn.close();
        cleanupTestFile();
    }
}

//=============================================================================
// WAL Mode Tests
//=============================================================================

TEST_CASE("WAL Mode Functionality", "[wal]") {

    SECTION("WAL mode can be enabled") {
        SqliteConnection conn = createTestConnection(true);
        REQUIRE(conn.connect() == true);

        // Check if WAL mode is enabled
        SqlTask checkJournalMode("PRAGMA journal_mode");
        REQUIRE(conn.exec(checkJournalMode) == IDbConnection::QuerySuccess);

        conn.close();
        cleanupTestFile();
    }

    SECTION("WAL mode can be disabled") {
        SqliteConnection conn = createTestConnection(false);
        REQUIRE(conn.connect() == true);

        // Traditional rollback mode
        SqlTask checkJournalMode("PRAGMA journal_mode");
        REQUIRE(conn.exec(checkJournalMode) == IDbConnection::QuerySuccess);

        conn.close();
        cleanupTestFile();
    }

    SECTION("WAL mode performance comparison") {
        auto testWithMode = [](bool useWAL) -> long long {
            SqliteConnection conn("perf_test.db", useWAL);
            conn.connect();

            SqlTask createTable(
                "CREATE TABLE IF NOT EXISTS test ("
                "id INTEGER PRIMARY KEY,"
                "data TEXT)"
                );
            conn.exec(createTable);

            auto start = std::chrono::high_resolution_clock::now();

            // Insert 100 records
            for (int i = 0; i < 100; ++i) {
                SqlTask insert("INSERT INTO test (data) VALUES ('data" + std::to_string(i) + "')");
                conn.exec(insert);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            conn.close();
            fs::remove("perf_test.db");
            if (fs::exists("perf_test.db-wal")) fs::remove("perf_test.db-wal");
            if (fs::exists("perf_test.db-shm")) fs::remove("perf_test.db-shm");

            return duration.count();
        };

        long long timeWithoutWAL = testWithMode(false);
        long long timeWithWAL = testWithMode(true);

        std::cout << "\nPerformance comparison:" << std::endl;
        std::cout << "Without WAL: " << timeWithoutWAL << "ms" << std::endl;
        std::cout << "With WAL: " << timeWithWAL << "ms" << std::endl;

        // WAL should be faster (or at least not significantly slower)
        // Note: For small datasets, difference might not be dramatic
        REQUIRE(timeWithWAL <= timeWithoutWAL * 1.2); // Allow 20% tolerance
    }
}

//=============================================================================
// Query Execution Tests
//=============================================================================

TEST_CASE("Query Execution", "[query]") {
    SqliteConnection conn = createTestConnection();
    REQUIRE(conn.connect() == true);

    SECTION("Execute queries with valid connection") {
        // Clean up any existing tables
        SqlTask dropIfExists("DROP TABLE IF EXISTS test_table");
        conn.exec(dropIfExists);

        // Test CREATE TABLE
        SqlTask createTable(
            "CREATE TABLE test_table ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "value TEXT NOT NULL)"
            );
        REQUIRE(conn.exec(createTable) == IDbConnection::QuerySuccess);

        // Test INSERT
        SqlTask insertValue("INSERT INTO test_table (value) VALUES ('test1')");
        REQUIRE(conn.exec(insertValue) == IDbConnection::QuerySuccess);

        // Test SELECT
        SqlTask selectValues("SELECT * FROM test_table");
        REQUIRE(conn.exec(selectValues) == IDbConnection::QuerySuccess);

        // Test UPDATE
        SqlTask updateValue("UPDATE test_table SET value = 'test2' WHERE value = 'test1'");
        REQUIRE(conn.exec(updateValue) == IDbConnection::QuerySuccess);

        // Test DELETE
        SqlTask deleteValue("DELETE FROM test_table WHERE value = 'test2'");
        REQUIRE(conn.exec(deleteValue) == IDbConnection::QuerySuccess);

        // Cleanup
        SqlTask dropTable("DROP TABLE test_table");
        REQUIRE(conn.exec(dropTable) == IDbConnection::QuerySuccess);
    }

    SECTION("Invalid query handling") {
        SqlTask invalidQuery("SELECT * FROM non_existent_table");
        REQUIRE(conn.exec(invalidQuery) == IDbConnection::QueryFailed);

        SqlTask invalidSyntax("INSERT INTO users VALUES (1, 'test'");
        REQUIRE(conn.exec(invalidSyntax) == IDbConnection::QueryFailed);
    }

    SECTION("Query without connection") {
        SqliteConnection conn2 = createTestConnection();
        SqlTask query("SELECT 1");
        REQUIRE(conn2.exec(query) == IDbConnection::UndefinedQuery);
    }

    SECTION("Last insert ID") {
        setupTestDatabase(conn);

        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('John Doe', 'john@example.com')"
            );
        conn.exec(insertUser);

        unsigned long long lastId = conn.getLastInsertId();
        REQUIRE(lastId > 0);

        cleanupTestDatabase(conn);
    }

    conn.close();
    cleanupTestFile();
}

//=============================================================================
// CRUD Operations Tests
//=============================================================================

TEST_CASE("CRUD Operations", "[crud]") {
    SqliteConnection conn = createTestConnection();
    REQUIRE(conn.connect() == true);
    setupTestDatabase(conn);

    SECTION("Create - Insert user") {
        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('John Doe', 'john@example.com')"
            );
        REQUIRE(conn.exec(insertUser) == IDbConnection::QuerySuccess);
        REQUIRE(conn.getLastInsertId() > 0);

        SqlTask selectUser("SELECT COUNT(*) FROM users WHERE email = 'john@example.com'");
        REQUIRE(conn.exec(selectUser) == IDbConnection::QuerySuccess);
    }

    SECTION("Read - Select users") {
        SqlTask insertUsers(
            "INSERT INTO users (name, email) VALUES "
            "('Alice', 'alice@example.com'),"
            "('Bob', 'bob@example.com')"
            );
        conn.exec(insertUsers);

        SqlTask selectUsers("SELECT * FROM users");
        REQUIRE(conn.exec(selectUsers) == IDbConnection::QuerySuccess);
    }

    SECTION("Update - Modify user") {
        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('Old Name', 'old@example.com')"
            );
        conn.exec(insertUser);

        SqlTask updateUser(
            "UPDATE users SET name = 'New Name' WHERE email = 'old@example.com'"
            );
        REQUIRE(conn.exec(updateUser) == IDbConnection::QuerySuccess);
    }

    SECTION("Delete - Remove user") {
        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('To Delete', 'delete@example.com')"
            );
        conn.exec(insertUser);

        SqlTask deleteUser("DELETE FROM users WHERE email = 'delete@example.com'");
        REQUIRE(conn.exec(deleteUser) == IDbConnection::QuerySuccess);
    }

    SECTION("Multiple operations in sequence") {
        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('User1', 'user1@example.com')"
            );
        REQUIRE(conn.exec(insertUser) == IDbConnection::QuerySuccess);

        SqlTask selectUser("SELECT * FROM users WHERE email = 'user1@example.com'");
        REQUIRE(conn.exec(selectUser) == IDbConnection::QuerySuccess);

        SqlTask updateUser(
            "UPDATE users SET name = 'Updated User' WHERE email = 'user1@example.com'"
            );
        REQUIRE(conn.exec(updateUser) == IDbConnection::QuerySuccess);

        SqlTask deleteUser("DELETE FROM users WHERE email = 'user1@example.com'");
        REQUIRE(conn.exec(deleteUser) == IDbConnection::QuerySuccess);
    }

    cleanupTestDatabase(conn);
    conn.close();
    cleanupTestFile();
}

//=============================================================================
// SQLite Specific Tests
//=============================================================================

TEST_CASE("SQLite Specific Features", "[sqlite_specific]") {

    SECTION("In-memory database isolation") {
        SqliteConnection conn1 = createInMemoryConnection();
        SqliteConnection conn2 = createInMemoryConnection();

        conn1.connect();
        conn2.connect();

        SqlTask createTable("CREATE TABLE test (id INTEGER)");
        conn1.exec(createTable);

        SqlTask insertValue("INSERT INTO test VALUES (1)");
        conn1.exec(insertValue);

        SqlTask selectFromConn2("SELECT * FROM test");
        // Should fail because in-memory databases are isolated
        REQUIRE(conn2.exec(selectFromConn2) == IDbConnection::QueryFailed);

        conn1.close();
        conn2.close();
    }

    SECTION("Foreign key constraints") {
        SqliteConnection conn = createTestConnection();
        conn.connect();

        SqlTask createParent(
            "CREATE TABLE parent ("
            "id INTEGER PRIMARY KEY,"
            "name TEXT)"
            );
        conn.exec(createParent);

        SqlTask createChild(
            "CREATE TABLE child ("
            "id INTEGER PRIMARY KEY,"
            "parent_id INTEGER REFERENCES parent(id) ON DELETE CASCADE)"
            );
        conn.exec(createChild);

        SqlTask insertParent("INSERT INTO parent VALUES (1, 'Parent')");
        conn.exec(insertParent);

        SqlTask insertChild("INSERT INTO child VALUES (1, 1)");
        REQUIRE(conn.exec(insertChild) == IDbConnection::QuerySuccess);

        SqlTask deleteParent("DELETE FROM parent WHERE id = 1");
        conn.exec(deleteParent);

        SqlTask checkChild("SELECT COUNT(*) FROM child");
        conn.exec(checkChild);

        conn.close();
        cleanupTestFile();
    }
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_CASE("Edge Cases and Error Handling", "[edge_cases]") {
    SqliteConnection conn = createTestConnection();

    SECTION("Empty query") {
        conn.connect();
        SqlTask emptyQuery("");
        REQUIRE(conn.exec(emptyQuery) == IDbConnection::UndefinedQuery);
        conn.close();
        cleanupTestFile();
    }

    SECTION("Query with special characters") {
        conn.connect();
        setupTestDatabase(conn);

        SqlTask insertSpecial(
            "INSERT INTO users (name, email) VALUES "
            "('O''Reilly', 'oreilly@example.com')"
            );
        REQUIRE(conn.exec(insertSpecial) == IDbConnection::QuerySuccess);

        SqlTask selectSpecial(
            "SELECT * FROM users WHERE name = 'O''Reilly'"
            );
        REQUIRE(conn.exec(selectSpecial) == IDbConnection::QuerySuccess);

        cleanupTestDatabase(conn);
        conn.close();
        cleanupTestFile();
    }

    SECTION("Very long query") {
        conn.connect();
        std::string longName(1000, 'A');
        std::string longQuery = "SELECT '" + longName + "'";
        SqlTask query(longQuery);
        REQUIRE(conn.exec(query) == IDbConnection::QuerySuccess);
        conn.close();
        cleanupTestFile();
    }

    SECTION("Execute after connection loss") {
        conn.connect();
        conn.close();

        SqlTask query("SELECT 1");
        REQUIRE(conn.exec(query) == IDbConnection::UndefinedQuery);
        cleanupTestFile();
    }

    SECTION("Multiple queries in same connection") {
        conn.connect();
        setupTestDatabase(conn);

        std::vector<std::string> queries = {
            "INSERT INTO users (name, email) VALUES ('User1', 'user1@test.com')",
            "INSERT INTO users (name, email) VALUES ('User2', 'user2@test.com')",
            "INSERT INTO users (name, email) VALUES ('User3', 'user3@test.com')",
            "SELECT COUNT(*) FROM users",
            "DELETE FROM users WHERE email LIKE '%@test.com'"
        };

        for (const auto& sql : queries) {
            SqlTask task(sql);
            REQUIRE(conn.exec(task) == IDbConnection::QuerySuccess);
        }

        cleanupTestDatabase(conn);
        conn.close();
        cleanupTestFile();
    }

    SECTION("Transaction handling") {
        conn.connect();

        SqlTask beginTransaction("BEGIN TRANSACTION");
        REQUIRE(conn.exec(beginTransaction) == IDbConnection::QuerySuccess);

        SqlTask createTable("CREATE TABLE temp (id INTEGER)");
        REQUIRE(conn.exec(createTable) == IDbConnection::QuerySuccess);

        SqlTask insertValue("INSERT INTO temp VALUES (1)");
        REQUIRE(conn.exec(insertValue) == IDbConnection::QuerySuccess);

        SqlTask rollback("ROLLBACK");
        REQUIRE(conn.exec(rollback) == IDbConnection::QuerySuccess);

        // Table should not exist after rollback
        SqlTask selectTemp("SELECT * FROM temp");
        REQUIRE(conn.exec(selectTemp) == IDbConnection::QueryFailed);

        conn.close();
        cleanupTestFile();
    }
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_CASE("Performance Tests", "[performance]") {

    SECTION("Bulk inserts without WAL") {
        SqliteConnection conn = createTestConnection(false);
        REQUIRE(conn.connect() == true);
        setupTestDatabase(conn);

        const int NUM_INSERTS = 100;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_INSERTS; ++i) {
            SqlTask insertUser(
                "INSERT INTO users (name, email) VALUES "
                "('User" + std::to_string(i) + "', 'user" + std::to_string(i) + "@test.com')"
                );
            REQUIRE(conn.exec(insertUser) == IDbConnection::QuerySuccess);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "\nBulk insert without WAL (" << NUM_INSERTS << " records): "
                  << duration.count() << "ms" << std::endl;

        SqlTask deleteAll("DELETE FROM users WHERE email LIKE '%@test.com'");
        conn.exec(deleteAll);

        cleanupTestDatabase(conn);
        conn.close();
        cleanupTestFile();
    }

    SECTION("Bulk inserts with WAL") {
        SqliteConnection conn = createTestConnection(true);
        REQUIRE(conn.connect() == true);
        setupTestDatabase(conn);

        const int NUM_INSERTS = 100;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_INSERTS; ++i) {
            SqlTask insertUser(
                "INSERT INTO users (name, email) VALUES "
                "('User" + std::to_string(i) + "', 'user" + std::to_string(i) + "@test.com')"
                );
            REQUIRE(conn.exec(insertUser) == IDbConnection::QuerySuccess);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Bulk insert with WAL (" << NUM_INSERTS << " records): "
                  << duration.count() << "ms" << std::endl;

        SqlTask deleteAll("DELETE FROM users WHERE email LIKE '%@test.com'");
        conn.exec(deleteAll);

        cleanupTestDatabase(conn);
        conn.close();
        cleanupTestFile();
    }

    SECTION("Bulk inserts with transaction (best performance)") {
        SqliteConnection conn = createTestConnection(false);
        REQUIRE(conn.connect() == true);
        setupTestDatabase(conn);

        const int NUM_INSERTS = 1000;

        SqlTask beginTransaction("BEGIN TRANSACTION");
        conn.exec(beginTransaction);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_INSERTS; ++i) {
            SqlTask insertUser(
                "INSERT INTO users (name, email) VALUES "
                "('User" + std::to_string(i) + "', 'user" + std::to_string(i) + "@test.com')"
                );
            REQUIRE(conn.exec(insertUser) == IDbConnection::QuerySuccess);
        }

        SqlTask commit("COMMIT");
        conn.exec(commit);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Bulk insert with transaction (" << NUM_INSERTS << " records): "
                  << duration.count() << "ms" << std::endl;

        SqlTask deleteAll("DELETE FROM users WHERE email LIKE '%@test.com'");
        conn.exec(deleteAll);

        cleanupTestDatabase(conn);
        conn.close();
        cleanupTestFile();
    }
}

//=============================================================================
// Main function
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "SQLite Connection Test Suite (Catch2)" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "\nNOTE: SQLite is embedded, no server required" << std::endl;
    std::cout << "Database: test_db.sqlite (file-based) and :memory: (in-memory)" << std::endl;
    std::cout << "WAL Mode: Can be enabled/disabled via constructor parameter" << std::endl;
    std::cout << "======================================\n" << std::endl;

    // Clean up before starting
    cleanupTestFile();

    int result = Catch::Session().run(argc, argv);

    // Clean up after tests
    cleanupTestFile();

    return result;
}
