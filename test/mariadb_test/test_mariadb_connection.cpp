#include "MariadbConnection.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>

using namespace std::chrono_literals;

// Helper function to create a test connection
MariadbConnection createTestConnection() {
    return MariadbConnection("127.0.0.1", "root", "1234", "test_db", 3306);
}

// Helper function to setup test database
void setupTestDatabase(MariadbConnection& conn) {
    if (!conn.healthy()) {
        conn.connect();
    }

    // Drop existing tables
    SqlTask dropUsers("DROP TABLE IF EXISTS users");
    conn.exec(dropUsers);
    SqlTask dropTestTable("DROP TABLE IF EXISTS test_table");
    conn.exec(dropTestTable);

    // Create users table
    SqlTask createUsersTable(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "name VARCHAR(100) NOT NULL,"
        "email VARCHAR(100) NOT NULL UNIQUE"
        ")"
        );
    conn.exec(createUsersTable);
}

// Helper function to cleanup test database
void cleanupTestDatabase(MariadbConnection& conn) {
    SqlTask dropUsers("DROP TABLE IF EXISTS users");
    conn.exec(dropUsers);
    SqlTask dropTestTable("DROP TABLE IF EXISTS test_table");
    conn.exec(dropTestTable);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_CASE("Connection Management", "[connection]") {

    SECTION("Successful connection") {
        MariadbConnection conn = createTestConnection();

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);
    }

    SECTION("Failed connection - invalid host") {
        MariadbConnection conn("invalid_host", "root", "password", "test_db", 3306);
        REQUIRE(conn.connect() == false);
    }

    SECTION("Failed connection - invalid credentials") {
        MariadbConnection conn("127.0.0.1", "invalid_user", "invalid_pass", "test_db", 3306);
        REQUIRE(conn.connect() == false);
    }

    SECTION("Failed connection - invalid port") {
        MariadbConnection conn("127.0.0.1", "root", "1234", "test_db", 9999);
        REQUIRE(conn.connect() == false);
    }

    SECTION("Reconnection behavior") {
        MariadbConnection conn = createTestConnection();

        REQUIRE(conn.connect() == true);
        REQUIRE(conn.connect() == true); // Second attempt should succeed

        conn.close();
        REQUIRE(conn.connect() == true); // Reconnect after close

        conn.close();
    }

    SECTION("Connection health states") {
        MariadbConnection conn = createTestConnection();

        REQUIRE(conn.healthy() == false);

        conn.connect();
        REQUIRE(conn.healthy() == true);

        conn.close();
        REQUIRE(conn.healthy() == false);
    }

    SECTION("Multiple connections") {
        const int NUM_CONNECTIONS = 5;
        std::vector<std::unique_ptr<MariadbConnection>> connections;

        for (int i = 0; i < NUM_CONNECTIONS; ++i) {
            auto conn = std::make_unique<MariadbConnection>(
                "127.0.0.1", "root", "1234", "test_db", 3306
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

        MariadbConnection conn("192.168.99.99", "root", "password", "test_db", 3306);
        bool connected = conn.connect();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

        REQUIRE(connected == false);
        REQUIRE(duration.count() < 15);
    }

    SECTION("Resource cleanup - RAII") {
        {
            MariadbConnection conn = createTestConnection();
            conn.connect();
            REQUIRE(conn.healthy() == true);
        }

        MariadbConnection conn2 = createTestConnection();
        REQUIRE(conn2.connect() == true);
        conn2.close();
    }

    SECTION("Connection without database specified") {
        MariadbConnection conn("127.0.0.1", "root", "1234", "", 3306);
        REQUIRE(conn.connect() == true);
        REQUIRE(conn.healthy() == true);
        conn.close();
    }
}

//=============================================================================
// Query Execution Tests
//=============================================================================

TEST_CASE("Query Execution", "[query]") {
    MariadbConnection conn = createTestConnection();
    REQUIRE(conn.connect() == true);

    SECTION("Execute queries with valid connection") {
        // Clean up any existing tables
        SqlTask dropIfExists("DROP TABLE IF EXISTS test_table");
        conn.exec(dropIfExists);

        // Test CREATE TABLE
        SqlTask createTable(
            "CREATE TABLE test_table ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
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
        MariadbConnection conn2 = createTestConnection();
        SqlTask query("SELECT 1");
        REQUIRE(conn2.exec(query) == IDbConnection::UndefinedQuery);
    }

    conn.close();
}

//=============================================================================
// CRUD Operations Tests
//=============================================================================

TEST_CASE("CRUD Operations", "[crud]") {
    MariadbConnection conn = createTestConnection();
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
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_CASE("Edge Cases and Error Handling", "[edge_cases]") {
    MariadbConnection conn = createTestConnection();

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
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_CASE("Performance Tests", "[performance]") {
    MariadbConnection conn = createTestConnection();
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

    // Clean up after performance test
    SqlTask deleteAll("DELETE FROM users WHERE email LIKE '%@test.com'");
    conn.exec(deleteAll);

    cleanupTestDatabase(conn);
    conn.close();
}

//=============================================================================
// Main function
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "======================================" << std::endl;
    std::cout << "MariaDB Connection Test Suite (Catch2)" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "\nNOTE: Make sure MariaDB/MySQL is running" << std::endl;
    std::cout << "Connection: 127.0.0.1:3306, user: root, password: 1234" << std::endl;
    std::cout << "Database: test_db" << std::endl;
    std::cout << "======================================\n" << std::endl;

    return Catch::Session().run(argc, argv);
}
