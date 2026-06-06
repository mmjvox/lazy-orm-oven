#ifndef DBCONFIGS_H
#define DBCONFIGS_H

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

struct MariaDBConfig {
    // تنظیمات اصلی اتصال
    std::string host = "localhost";
    std::string user = "root";
    std::string password = "";
    std::string database = "";
    unsigned int port = 3306;
    std::string socket = "";

    // تنظیمات پیشرفته
    unsigned int connectionTimeout = 10;     // seconds
    unsigned int readTimeout = 30;            // seconds
    unsigned int writeTimeout = 30;           // seconds

    // تنظیمات اتصال مجدد
    bool autoReconnect = true;
    unsigned int maxRetries = 3;
    unsigned int retryDelay = 1;              // seconds

    // تنظیمات اتصال‌های همزمان
    unsigned int maxConnections = 10;
    unsigned int minConnections = 2;
    unsigned int connectionLifetime = 3600;   // seconds

    // تنظیمات character set
    std::string charset = "utf8mb4";
    std::string collation = "utf8mb4_unicode_ci";

    // تنظیمات SSL/TLS
    struct SSLConfig {
        bool enabled = false;
        std::string caPath = "";
        std::string certPath = "";
        std::string keyPath = "";
    } ssl;

    // تنظیمات پول اتصال
    struct PoolConfig {
        bool enabled = true;
        unsigned int maxIdleConnections = 5;
        unsigned int idleTimeout = 300;        // seconds
        bool verifyOnBorrow = true;
    } connectionPool;

    // تنظیمات لاگینگ
    struct LoggingConfig {
        bool enabled = true;
        std::string logLevel = "info";
        std::string logFile = "";
        bool logQueries = false;
        bool logSlowQueries = true;
        unsigned int slowQueryThreshold = 2;   // seconds
    } logging;

    // پراپرتی‌های اضافی
    std::unordered_map<std::string, std::string> extraProperties;

    // متدهای کمکی

    // اعتبارسنجی تنظیمات
    bool isValid() const {
        return !host.empty() &&
               !user.empty() &&
               port > 0 &&
               port <= 65535 &&
               !charset.empty();
    }

    // ساختن URL اتصال
    std::string getConnectionString() const {
        std::string connStr = "mysql://" + user;
        if (!password.empty()) {
            connStr += ":" + password;
        }
        connStr += "@" + host + ":" + std::to_string(port);
        if (!database.empty()) {
            connStr += "/" + database;
        }
        return connStr;
    }

    // تنظیمات برای لاگینگ
    std::string toLogString() const {
        std::string log = "MariaDB Config:\n";
        log += "  Host: " + host + "\n";
        log += "  User: " + user + "\n";
        log += "  Port: " + std::to_string(port) + "\n";
        log += "  Database: " + (database.empty() ? "none" : database) + "\n";
        log += "  Charset: " + charset + "\n";
        log += "  Auto Reconnect: " + std::string(autoReconnect ? "yes" : "no") + "\n";
        log += "  Connection Pool: " + std::string(connectionPool.enabled ? "enabled" : "disabled") + "\n";
        log += "  SSL: " + std::string(ssl.enabled ? "enabled" : "disabled") + "\n";
        return log;
    }

    // متد برای تنظیم از روی map
    void fromMap(const std::unordered_map<std::string, std::string>& config) {
        if (config.find("host") != config.end()) host = config.at("host");
        if (config.find("user") != config.end()) user = config.at("user");
        if (config.find("password") != config.end()) password = config.at("password");
        if (config.find("database") != config.end()) database = config.at("database");
        if (config.find("port") != config.end()) port = std::stoi(config.at("port"));
        if (config.find("charset") != config.end()) charset = config.at("charset");
        if (config.find("autoReconnect") != config.end()) autoReconnect = (config.at("autoReconnect") == "true");
        if (config.find("connectionTimeout") != config.end()) connectionTimeout = std::stoi(config.at("connectionTimeout"));
        if (config.find("readTimeout") != config.end()) readTimeout = std::stoi(config.at("readTimeout"));
        if (config.find("writeTimeout") != config.end()) writeTimeout = std::stoi(config.at("writeTimeout"));
        if (config.find("maxConnections") != config.end()) maxConnections = std::stoi(config.at("maxConnections"));
        if (config.find("minConnections") != config.end()) minConnections = std::stoi(config.at("minConnections"));

        // SSL config
        if (config.find("ssl_enabled") != config.end()) ssl.enabled = (config.at("ssl_enabled") == "true");
        if (config.find("ssl_ca") != config.end()) ssl.caPath = config.at("ssl_ca");
        if (config.find("ssl_cert") != config.end()) ssl.certPath = config.at("ssl_cert");
        if (config.find("ssl_key") != config.end()) ssl.keyPath = config.at("ssl_key");

        // اضافه کردن پراپرتی‌های اضافی
        extraProperties = config;
    }

    // متد برای تبدیل به map
    std::unordered_map<std::string, std::string> toMap() const {
        std::unordered_map<std::string, std::string> config;
        config["host"] = host;
        config["user"] = user;
        config["password"] = password;
        config["database"] = database;
        config["port"] = std::to_string(port);
        config["charset"] = charset;
        config["collation"] = collation;
        config["autoReconnect"] = autoReconnect ? "true" : "false";
        config["connectionTimeout"] = std::to_string(connectionTimeout);
        config["readTimeout"] = std::to_string(readTimeout);
        config["writeTimeout"] = std::to_string(writeTimeout);
        config["maxConnections"] = std::to_string(maxConnections);
        config["minConnections"] = std::to_string(minConnections);
        config["maxRetries"] = std::to_string(maxRetries);
        config["retryDelay"] = std::to_string(retryDelay);
        config["connectionLifetime"] = std::to_string(connectionLifetime);

        // SSL config
        config["ssl_enabled"] = ssl.enabled ? "true" : "false";
        config["ssl_ca"] = ssl.caPath;
        config["ssl_cert"] = ssl.certPath;
        config["ssl_key"] = ssl.keyPath;

        // Connection pool config
        config["pool_enabled"] = connectionPool.enabled ? "true" : "false";
        config["max_idle_connections"] = std::to_string(connectionPool.maxIdleConnections);
        config["idle_timeout"] = std::to_string(connectionPool.idleTimeout);
        config["verify_on_borrow"] = connectionPool.verifyOnBorrow ? "true" : "false";

        // Logging config
        config["logging_enabled"] = logging.enabled ? "true" : "false";
        config["log_level"] = logging.logLevel;
        config["log_file"] = logging.logFile;
        config["log_queries"] = logging.logQueries ? "true" : "false";
        config["log_slow_queries"] = logging.logSlowQueries ? "true" : "false";
        config["slow_query_threshold"] = std::to_string(logging.slowQueryThreshold);

        return config;
    }
};

#endif // DBCONFIGS_H
