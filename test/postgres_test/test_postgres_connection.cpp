#include "PostgresConnection.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

using namespace std::chrono_literals;
using namespace LazyOrm;

// Global mutex for thread-safe database setup
static std::mutex dbSetupMutex;

// Helper function to create a test connection
PostgresConnection createTestConnection() {
    return PostgresConnection("127.0.0.1", "postgres", "1234", "test_db", 5432);
}

// Helper function to setup test database (thread-safe)
void setupTestDatabase(PostgresConnection& conn) {
    if (!conn.healthy()) {
        conn.connect();
    }

    // Drop existing tables (CASCADE for PostgreSQL to handle dependencies)
    SqlTask dropUsers("DROP TABLE IF EXISTS users CASCADE");
    conn.exec(dropUsers);
    SqlTask dropTestTable("DROP TABLE IF EXISTS test_table CASCADE");
    conn.exec(dropTestTable);
    SqlTask dropOrders("DROP TABLE IF EXISTS orders CASCADE");
    conn.exec(dropOrders);
    SqlTask dropSequence("DROP SEQUENCE IF EXISTS users_id_seq CASCADE");
    conn.exec(dropSequence);

    // Create users table (PostgreSQL uses SERIAL for auto-increment)
    SqlTask createUsersTable(
        "CREATE TABLE IF NOT EXISTS users ("
        "id SERIAL PRIMARY KEY,"
        "name VARCHAR(100) NOT NULL,"
        "email VARCHAR(100) NOT NULL UNIQUE"
        ")"
        );
    conn.exec(createUsersTable);
}

// Helper function to setup test database with orders table
void setupTestDatabaseWithOrders(PostgresConnection& conn) {
    setupTestDatabase(conn);

    // Create orders table
    SqlTask createOrders(
        "CREATE TABLE IF NOT EXISTS orders ("
        "id SERIAL PRIMARY KEY,"
        "user_id INTEGER REFERENCES users(id),"
        "product VARCHAR(100))"
        );
    conn.exec(createOrders);
}

// Helper function to cleanup test database
void cleanupTestDatabase(PostgresConnection& conn) {
    SqlTask dropOrders("DROP TABLE IF EXISTS orders CASCADE");
    conn.exec(dropOrders);
    SqlTask dropUsers("DROP TABLE IF EXISTS users CASCADE");
    conn.exec(dropUsers);
    SqlTask dropTestTable("DROP TABLE IF EXISTS test_table CASCADE");
    conn.exec(dropTestTable);
}

// Helper function to insert test data for JOIN tests
void insertTestData(PostgresConnection& conn) {
    // Insert test users
    SqlTask insertUsers(
        "INSERT INTO users (id, name, email) VALUES "
        "(1, 'John Doe', 'john@example.com'),"
        "(2, 'Jane Smith', 'jane@example.com'),"
        "(3, 'Bob Johnson', 'bob@example.com') "
        "ON CONFLICT (id) DO NOTHING"
        );
    conn.exec(insertUsers);

    // Insert test orders
    SqlTask insertOrders(
        "INSERT INTO orders (user_id, product) VALUES "
        "(1, 'Product A'),"
        "(1, 'Product B'),"
        "(2, 'Product C'),"
        "(3, 'Product D') "
        "ON CONFLICT DO NOTHING"
        );
    conn.exec(insertOrders);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_CASE("Connection Management", "[connection]") {

    SECTION("Successful connection") {
        PostgresConnection conn = createTestConnection();

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);
    }

    SECTION("Failed connection - invalid host") {
        PostgresConnection conn("invalid_host", "postgres", "1234", "test_db", 5432);
        REQUIRE(conn.connect() == false);
    }

    SECTION("Failed connection - invalid credentials") {
        PostgresConnection conn("127.0.0.1", "invalid_user", "invalid_pass", "test_db", 5432);
        REQUIRE(conn.connect() == false);
    }

    SECTION("Failed connection - invalid port") {
        PostgresConnection conn("127.0.0.1", "postgres", "1234", "test_db", 9999);
        REQUIRE(conn.connect() == false);
    }

    SECTION("Failed connection - non-existent database") {
        PostgresConnection conn("127.0.0.1", "postgres", "1234", "non_existent_db", 5432);
        REQUIRE(conn.connect() == false);
    }

    SECTION("Reconnection behavior") {
        PostgresConnection conn = createTestConnection();

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.connect() == true); // Second attempt should succeed

        conn.close();
        REQUIRE(conn.connect() == true); // Reconnect after close

        conn.close();
    }

    SECTION("Connection health states") {
        PostgresConnection conn = createTestConnection();

        REQUIRE(conn.healthy() == false);

        conn.connect();
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);
    }

    SECTION("Multiple connections") {
        const int NUM_CONNECTIONS = 5;
        std::vector<std::unique_ptr<PostgresConnection>> connections;

        for (int i = 0; i < NUM_CONNECTIONS; ++i) {
            auto conn = std::make_unique<PostgresConnection>(
                "127.0.0.1", "postgres", "1234", "test_db", 5432
                );
            REQUIRE(conn->connect() == true);
            connections.push_back(std::move(conn));
        }

        for (auto& conn : connections) {
            REQUIRE(conn->healthy() == true);
            conn->close();
        }
    }

    SECTION("Connection timeout") {
        auto start = std::chrono::high_resolution_clock::now();

        PostgresConnection conn("192.168.99.99", "postgres", "1234", "test_db", 5432);
        bool connected = conn.connect();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

        REQUIRE(connected == false);
        REQUIRE(duration.count() < 15); // Should timeout within 15 seconds
    }

    SECTION("Resource cleanup - RAII") {
        {
            PostgresConnection conn = createTestConnection();
            conn.connect();
            REQUIRE(conn.healthy() == true);
        }

        PostgresConnection conn2 = createTestConnection();
        REQUIRE(conn2.connect() == true);
        conn2.close();
    }

    SECTION("Connection with SSL") {
        PostgresConnection conn("127.0.0.1", "postgres", "1234", "test_db", 5432);
        conn.setSslMode("disable");
        REQUIRE(conn.connect() == true);
        conn.close();

        // Try require SSL (might fail if SSL not configured)
        PostgresConnection conn2("127.0.0.1", "postgres", "1234", "test_db", 5432);
        conn2.setSslMode("require");
        // This might succeed or fail depending on PostgreSQL SSL configuration
        bool result = conn2.connect();
        if (!result) {
            std::cout << "Note: SSL connection failed (SSL may not be configured on server)" << std::endl;
        }
        conn2.close();
    }
}

//=============================================================================
// Query Execution Tests
//=============================================================================

TEST_CASE("Query Execution", "[query]") {
    PostgresConnection conn = createTestConnection();
    REQUIRE(conn.connect() == true);

    SECTION("Execute queries with valid connection") {
        // Clean up any existing tables
        SqlTask dropIfExists("DROP TABLE IF EXISTS test_table CASCADE");
        conn.exec(dropIfExists);

        // Test CREATE TABLE (PostgreSQL uses SERIAL for auto-increment)
        SqlTask createTable(
            "CREATE TABLE test_table ("
            "id SERIAL PRIMARY KEY,"
            "value VARCHAR(100) NOT NULL)"
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
        PostgresConnection conn2 = createTestConnection();
        SqlTask query("SELECT 1");
        REQUIRE(conn2.exec(query) == IDbConnection::UndefinedQuery);
    }

    SECTION("Complex query with JOIN") {
        // Setup database with orders table and test data
        setupTestDatabaseWithOrders(conn);
        insertTestData(conn);

        // Test JOIN query
        SqlTask joinQuery(
            "SELECT u.name, o.product FROM users u "
            "INNER JOIN orders o ON u.id = o.user_id "
            "ORDER BY u.id, o.id"
            );
        REQUIRE(conn.exec(joinQuery) == IDbConnection::QuerySuccess);

        // Cleanup
        cleanupTestDatabase(conn);
    }

    conn.close();
}

//=============================================================================
// CRUD Operations Tests
//=============================================================================

TEST_CASE("CRUD Operations", "[crud]") {
    PostgresConnection conn = createTestConnection();
    REQUIRE(conn.connect() == true);
    setupTestDatabase(conn);

    SECTION("Create - Insert user") {
        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('John Doe', 'john@example.com')"
            );
        REQUIRE(conn.exec(insertUser) == IDbConnection::QuerySuccess);

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

        SqlTask selectUsers("SELECT * FROM users ORDER BY id");
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

        // Verify update
        SqlTask verifyUpdate("SELECT name FROM users WHERE email = 'old@example.com'");
        REQUIRE(conn.exec(verifyUpdate) == IDbConnection::QuerySuccess);
    }

    SECTION("Delete - Remove user") {
        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('To Delete', 'delete@example.com')"
            );
        conn.exec(insertUser);

        SqlTask deleteUser("DELETE FROM users WHERE email = 'delete@example.com'");
        REQUIRE(conn.exec(deleteUser) == IDbConnection::QuerySuccess);

        // Verify deletion
        SqlTask verifyDelete("SELECT COUNT(*) FROM users WHERE email = 'delete@example.com'");
        REQUIRE(conn.exec(verifyDelete) == IDbConnection::QuerySuccess);
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
}

//=============================================================================
// PostgreSQL Specific Features Tests
//=============================================================================

TEST_CASE("PostgreSQL Specific Features", "[postgres]") {
    PostgresConnection conn = createTestConnection();
    REQUIRE(conn.connect() == true);
    setupTestDatabase(conn);

    SECTION("Last Insert ID with SERIAL") {
        SqlTask insertUser(
            "INSERT INTO users (name, email) VALUES ('Test User', 'test@example.com')"
            );
        REQUIRE(conn.exec(insertUser) == IDbConnection::QuerySuccess);

        unsigned long long lastId = conn.getLastInsertId();
        REQUIRE(lastId > 0);

        std::cout << "Last Insert ID (PostgreSQL SERIAL): " << lastId << std::endl;
    }

    SECTION("Returning clause support") {
        SqlTask insertWithReturning(
            "INSERT INTO users (name, email) VALUES "
            "('Returning User', 'returning@example.com') "
            "RETURNING id"
            );
        REQUIRE(conn.exec(insertWithReturning) == IDbConnection::QuerySuccess);
    }

    SECTION("Transaction support") {
        // Start transaction
        SqlTask begin("BEGIN");
        REQUIRE(conn.exec(begin) == IDbConnection::QuerySuccess);

        SqlTask insert("INSERT INTO users (name, email) VALUES ('Txn User', 'txn@example.com')");
        REQUIRE(conn.exec(insert) == IDbConnection::QuerySuccess);

        // Rollback
        SqlTask rollback("ROLLBACK");
        REQUIRE(conn.exec(rollback) == IDbConnection::QuerySuccess);

        // Verify rollback
        SqlTask verify("SELECT COUNT(*) FROM users WHERE email = 'txn@example.com'");
        REQUIRE(conn.exec(verify) == IDbConnection::QuerySuccess);
    }

    SECTION("Array data type support") {
        SqlTask createArrayTable(
            "CREATE TABLE array_test ("
            "id SERIAL PRIMARY KEY,"
            "tags TEXT[])"
            );
        conn.exec(createArrayTable);

        SqlTask insertArray(
            "INSERT INTO array_test (tags) VALUES (ARRAY['tag1', 'tag2', 'tag3'])"
            );
        REQUIRE(conn.exec(insertArray) == IDbConnection::QuerySuccess);

        SqlTask selectArray("SELECT tags FROM array_test");
        REQUIRE(conn.exec(selectArray) == IDbConnection::QuerySuccess);

        SqlTask dropArray("DROP TABLE array_test CASCADE");
        conn.exec(dropArray);
    }

    SECTION("JSON/JSONB data type support") {
        SqlTask createJsonTable(
            "CREATE TABLE json_test ("
            "id SERIAL PRIMARY KEY,"
            "data JSONB)"
            );
        conn.exec(createJsonTable);

        SqlTask insertJson(
            "INSERT INTO json_test (data) VALUES "
            "('{\"name\": \"John\", \"age\": 30}'::jsonb)"
            );
        REQUIRE(conn.exec(insertJson) == IDbConnection::QuerySuccess);

        SqlTask selectJson("SELECT data->>'name' FROM json_test");
        REQUIRE(conn.exec(selectJson) == IDbConnection::QuerySuccess);

        SqlTask dropJson("DROP TABLE json_test CASCADE");
        conn.exec(dropJson);
    }

    cleanupTestDatabase(conn);
    conn.close();
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_CASE("Edge Cases and Error Handling", "[edge_cases]") {
    PostgresConnection conn = createTestConnection();

    SECTION("Empty query") {
        conn.connect();
        SqlTask emptyQuery("");
        REQUIRE(conn.exec(emptyQuery) == IDbConnection::UndefinedQuery);
        conn.close();
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
    }

    SECTION("Very long query") {
        conn.connect();
        std::string longName(1000, 'A');
        std::string longQuery = "SELECT '" + longName + "'";
        SqlTask query(longQuery);
        REQUIRE(conn.exec(query) == IDbConnection::QuerySuccess);
        conn.close();
    }

    SECTION("Execute after connection loss") {
        conn.connect();
        conn.close();

        SqlTask query("SELECT 1");
        REQUIRE(conn.exec(query) == IDbConnection::UndefinedQuery);
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
    }

    SECTION("Case sensitivity handling") {
        conn.connect();
        setupTestDatabase(conn);

        // PostgreSQL is case-sensitive for identifiers unless quoted
        SqlTask insertLower(
            "INSERT INTO users (name, email) VALUES ('Case Test', 'Case@Test.com')"
            );
        REQUIRE(conn.exec(insertLower) == IDbConnection::QuerySuccess);

        // This should work with exact case
        SqlTask selectExact(
            "SELECT * FROM users WHERE email = 'Case@Test.com'"
            );
        REQUIRE(conn.exec(selectExact) == IDbConnection::QuerySuccess);

        cleanupTestDatabase(conn);
        conn.close();
    }

    SECTION("NULL value handling") {
        conn.connect();

        SqlTask createNullTable(
            "CREATE TABLE null_test ("
            "id SERIAL PRIMARY KEY,"
            "nullable_col VARCHAR(100))"
            );
        conn.exec(createNullTable);

        SqlTask insertNull("INSERT INTO null_test (nullable_col) VALUES (NULL)");
        REQUIRE(conn.exec(insertNull) == IDbConnection::QuerySuccess);

        SqlTask selectNull("SELECT * FROM null_test WHERE nullable_col IS NULL");
        REQUIRE(conn.exec(selectNull) == IDbConnection::QuerySuccess);

        SqlTask dropNull("DROP TABLE null_test CASCADE");
        conn.exec(dropNull);

        conn.close();
    }
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_CASE("Performance Tests", "[performance]") {
    PostgresConnection conn = createTestConnection();
    REQUIRE(conn.connect() == true);
    setupTestDatabase(conn);

    SECTION("Bulk inserts") {
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

        REQUIRE(duration.count() < 5000);
        std::cout << "Bulk insert of " << NUM_INSERTS << " records took: "
                  << duration.count() << "ms" << std::endl;
    }

    SECTION("Bulk insert with single statement") {
        // Clean up before test
        SqlTask deleteAllBefore("DELETE FROM users WHERE email LIKE '%@test.com'");
        conn.exec(deleteAllBefore);

        auto start = std::chrono::high_resolution_clock::now();

        std::string bulkInsert = "INSERT INTO users (name, email) VALUES ";
        for (int i = 0; i < 100; ++i) {
            if (i > 0) bulkInsert += ",";
            bulkInsert += "('User" + std::to_string(i) + "', 'user" + std::to_string(i) + "@test.com')";
        }

        SqlTask insertBulk(bulkInsert);
        REQUIRE(conn.exec(insertBulk) == IDbConnection::QuerySuccess);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Bulk insert (single statement) of 100 records took: "
                  << duration.count() << "ms" << std::endl;
    }

    SECTION("Select performance") {
        // Clean up before test
        SqlTask deleteAllBefore("DELETE FROM users WHERE email LIKE '%@test.com'");
        conn.exec(deleteAllBefore);

        // Insert test data first
        for (int i = 0; i < 1000; ++i) {
            SqlTask insertUser(
                "INSERT INTO users (name, email) VALUES "
                "('PerfUser" + std::to_string(i) + "', 'perf" + std::to_string(i) + "@test.com')"
                );
            conn.exec(insertUser);
        }

        auto start = std::chrono::high_resolution_clock::now();

        SqlTask selectAll("SELECT * FROM users WHERE email LIKE '%@test.com'");
        REQUIRE(conn.exec(selectAll) == IDbConnection::QuerySuccess);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Select of 1000 records took: " << duration.count() << "ms" << std::endl;
    }

    // Clean up
    SqlTask deleteAll("DELETE FROM users WHERE email LIKE '%@test.com'");
    conn.exec(deleteAll);

    cleanupTestDatabase(conn);
    conn.close();
}

//=============================================================================
// Concurrency Tests
//=============================================================================

TEST_CASE("Concurrency Tests", "[concurrency]") {

    SECTION("Multiple threads using separate connections") {
        const int NUM_THREADS = 4;
        std::vector<std::thread> threads;
        std::atomic<int> successCount{0};

        // Each thread uses its own connection with a unique table name
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&successCount, t]() {
                PostgresConnection conn = createTestConnection();

                if (conn.connect()) {
                    // Create a unique table name for each thread to avoid conflicts
                    std::string tableName = "users_thread_" + std::to_string(t);

                    SqlTask dropTable("DROP TABLE IF EXISTS " + tableName + " CASCADE");
                    conn.exec(dropTable);

                    SqlTask createTable(
                        "CREATE TABLE " + tableName + " ("
                                                      "id SERIAL PRIMARY KEY,"
                                                      "name VARCHAR(100) NOT NULL,"
                                                      "email VARCHAR(100) NOT NULL UNIQUE)"
                        );
                    conn.exec(createTable);

                    SqlTask insertUser(
                        "INSERT INTO " + tableName + " (name, email) VALUES "
                                                     "('ThreadUser" + std::to_string(t) + "', 'thread" +
                        std::to_string(t) + "@test.com')"
                        );

                    if (conn.exec(insertUser) == IDbConnection::QuerySuccess) {
                        successCount++;
                    }

                    SqlTask dropTable2("DROP TABLE " + tableName + " CASCADE");
                    conn.exec(dropTable2);

                    conn.close();
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(successCount == NUM_THREADS);
    }
}

//=============================================================================
// Main function
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "PostgreSQL Connection Test Suite (Catch2)" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "\nNOTE: Make sure PostgreSQL is running" << std::endl;
    std::cout << "Connection: 127.0.0.1:5432, user: postgres, password: 1234" << std::endl;
    std::cout << "Database: test_db" << std::endl;
    std::cout << "\nPostgreSQL specific features:" << std::endl;
    std::cout << "  - SERIAL auto-increment" << std::endl;
    std::cout << "  - RETURNING clause" << std::endl;
    std::cout << "  - Transaction support" << std::endl;
    std::cout << "  - Arrays and JSONB data types" << std::endl;
    std::cout << "======================================\n" << std::endl;

    return Catch::Session().run(argc, argv);
}
