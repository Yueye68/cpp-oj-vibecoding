#include <gtest/gtest.h>
#include "utils/config.h"

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::instance().reset();
    }
};

TEST_F(ConfigTest, LoadFromString_Basic) {
    Config& config = Config::instance();
    std::string yaml_content = R"(
database:
  host: localhost
  port: 3306
  username: testuser
  password: testpass
  name: testdb
  charset: utf8mb4
  collation: utf8mb4_unicode_ci

server:
  host: 127.0.0.1
  port: 9000

app:
  test_case_dir: /tmp/test_cases
  upload_dir: /tmp/uploads
  log_level: DEBUG
  log_file: /tmp/test.log
)";
    bool result = config.loadFromString(yaml_content);
    EXPECT_TRUE(result);

    EXPECT_EQ("localhost", config.database().host);
    EXPECT_EQ(3306, config.database().port);
    EXPECT_EQ("testuser", config.database().username);
    EXPECT_EQ("testpass", config.database().password);
    EXPECT_EQ("testdb", config.database().database);
    EXPECT_EQ("utf8mb4", config.database().charset);
    EXPECT_EQ("utf8mb4_unicode_ci", config.database().collation);

    EXPECT_EQ("127.0.0.1", config.server().host);
    EXPECT_EQ(9000, config.server().port);

    EXPECT_EQ("/tmp/test_cases", config.app().test_case_dir);
    EXPECT_EQ("/tmp/uploads", config.app().upload_dir);
    EXPECT_EQ("DEBUG", config.app().log_level);
    EXPECT_EQ("/tmp/test.log", config.app().log_file);
}

TEST_F(ConfigTest, LoadFromString_PartialConfig) {
    Config& config = Config::instance();
    std::string yaml_content = R"(
database:
  host: db.example.com
  port: 3307
)";
    bool result = config.loadFromString(yaml_content);
    EXPECT_TRUE(result);

    EXPECT_EQ("db.example.com", config.database().host);
    EXPECT_EQ(3307, config.database().port);
    EXPECT_EQ("root", config.database().username);
    EXPECT_EQ("", config.database().password);
}

TEST_F(ConfigTest, LoadFromString_DefaultValues) {
    Config& config = Config::instance();
    std::string yaml_content = "";
    bool result = config.loadFromString(yaml_content);
    EXPECT_TRUE(result);

    EXPECT_EQ("localhost", config.database().host);
    EXPECT_EQ(3306, config.database().port);
    EXPECT_EQ("0.0.0.0", config.server().host);
    EXPECT_EQ(8080, config.server().port);
    EXPECT_EQ("./test_cases", config.app().test_case_dir);
    EXPECT_EQ("./uploads", config.app().upload_dir);
}

TEST_F(ConfigTest, LoadFromString_WithComments) {
    Config& config = Config::instance();
    std::string yaml_content = R"(
# This is a comment
database:
  host: commented.db.com
  port: 3308
# Another comment
server:
  host: 0.0.0.0
)";
    bool result = config.loadFromString(yaml_content);
    EXPECT_TRUE(result);
    EXPECT_EQ("commented.db.com", config.database().host);
    EXPECT_EQ(3308, config.database().port);
}

TEST_F(ConfigTest, LoadFromString_WithQuotedValues) {
    Config& config = Config::instance();
    std::string yaml_content = R"(
database:
  password: "quoted_password"
  name: 'single_quoted_db'
)";
    bool result = config.loadFromString(yaml_content);
    EXPECT_TRUE(result);
    EXPECT_EQ("quoted_password", config.database().password);
    EXPECT_EQ("single_quoted_db", config.database().database);
}

TEST_F(ConfigTest, LoadFromString_WithWhitespace) {
    Config& config = Config::instance();
    std::string yaml_content = "   \n  database:  \n    host:   whitespace.db.com  \n  ";
    bool result = config.loadFromString(yaml_content);
    EXPECT_TRUE(result);
    EXPECT_EQ("whitespace.db.com", config.database().host);
}

TEST_F(ConfigTest, Singleton_Persistence) {
    Config& config1 = Config::instance();
    std::string yaml_content = R"(
database:
  host: singleton.test.com
)";
    config1.loadFromString(yaml_content);

    Config& config2 = Config::instance();
    EXPECT_EQ("singleton.test.com", config2.database().host);
}
