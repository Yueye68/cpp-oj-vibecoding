#include <gtest/gtest.h>
#include "utils/password.h"
#include <thread>
#include <vector>
#include <atomic>

class PasswordUtilTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PasswordUtilTest, Hash_NotEmpty) {
    std::string hash = PasswordUtil::hash("password123");
    EXPECT_FALSE(hash.empty());
}

TEST_F(PasswordUtilTest, Hash_Format_BcryptPrefix) {
    std::string hash = PasswordUtil::hash("password123");
    EXPECT_TRUE(hash.substr(0, 4) == "$2b$");
}

TEST_F(PasswordUtilTest, Hash_Format_Cost) {
    std::string hash = PasswordUtil::hash("password123");
    EXPECT_TRUE(hash.substr(4, 2) == "10");
}

TEST_F(PasswordUtilTest, Hash_Format_Length) {
    std::string hash = PasswordUtil::hash("password123");
    EXPECT_EQ(hash.length(), 60u);
}

TEST_F(PasswordUtilTest, Hash_DifferentPasswords_DifferentHashes) {
    std::string hash1 = PasswordUtil::hash("password1");
    std::string hash2 = PasswordUtil::hash("password2");
    EXPECT_NE(hash1, hash2);
}

TEST_F(PasswordUtilTest, Hash_SamePassword_DifferentSalts) {
    std::string hash1 = PasswordUtil::hash("samepassword");
    std::string hash2 = PasswordUtil::hash("samepassword");
    EXPECT_NE(hash1, hash2);
}

TEST_F(PasswordUtilTest, Hash_EmptyPassword) {
    std::string hash = PasswordUtil::hash("");
    EXPECT_FALSE(hash.empty());
    EXPECT_TRUE(hash.substr(0, 4) == "$2b$");
}

TEST_F(PasswordUtilTest, Hash_ReasonableLengthPassword) {
    std::string password(50, 'a');
    std::string hash = PasswordUtil::hash(password);
    EXPECT_FALSE(hash.empty());
    EXPECT_TRUE(hash.substr(0, 4) == "$2b$");
}

TEST_F(PasswordUtilTest, Verify_CorrectPassword) {
    std::string password = "mysecretpassword";
    std::string hash = PasswordUtil::hash(password);
    EXPECT_TRUE(PasswordUtil::verify(password, hash));
}

TEST_F(PasswordUtilTest, Verify_WrongPassword) {
    std::string password = "mysecretpassword";
    std::string hash = PasswordUtil::hash(password);
    EXPECT_FALSE(PasswordUtil::verify("wrongpassword", hash));
}

TEST_F(PasswordUtilTest, Verify_SamePasswordDifferentHashes) {
    std::string password = "samepassword";
    std::string hash1 = PasswordUtil::hash(password);
    std::string hash2 = PasswordUtil::hash(password);
    EXPECT_NE(hash1, hash2);
    EXPECT_TRUE(PasswordUtil::verify(password, hash1));
    EXPECT_TRUE(PasswordUtil::verify(password, hash2));
}

TEST_F(PasswordUtilTest, Verify_EmptyPassword) {
    std::string hash = PasswordUtil::hash("password123");
    EXPECT_FALSE(PasswordUtil::verify("", hash));
}

TEST_F(PasswordUtilTest, Verify_EmptyHash) {
    EXPECT_FALSE(PasswordUtil::verify("password123", ""));
}

TEST_F(PasswordUtilTest, Verify_InvalidHashFormat) {
    EXPECT_FALSE(PasswordUtil::verify("password123", "invalidhash"));
    EXPECT_FALSE(PasswordUtil::verify("password123", "*invalid"));
    EXPECT_FALSE(PasswordUtil::verify("password123", "$2x$invalid"));
}

TEST_F(PasswordUtilTest, Verify_ValidBcryptHashes) {
    std::vector<std::string> passwords = {
        "password",
        "123456",
        "P@ssw0rd!",
        "admin",
        "test_user_123"
    };

    for (const auto& pwd : passwords) {
        std::string hash = PasswordUtil::hash(pwd);
        EXPECT_TRUE(PasswordUtil::verify(pwd, hash))
            << "Failed for password: " << pwd;
    }
}

TEST_F(PasswordUtilTest, Concurrent_DifferentPasswords) {
    const int num_threads = 4;
    const int ops_per_thread = 10;
    std::atomic<int> success_count{0};

    auto worker = [&success_count](int thread_id) {
        for (int i = 0; i < ops_per_thread; ++i) {
            std::string password = "password" + std::to_string(thread_id) + "_" + std::to_string(i);
            std::string hash = PasswordUtil::hash(password);
            if (PasswordUtil::verify(password, hash)) {
                success_count++;
            }

            std::string wrong_pwd = password + "wrong";
            if (!PasswordUtil::verify(wrong_pwd, hash)) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads * ops_per_thread * 2);
}

TEST_F(PasswordUtilTest, Concurrent_SamePassword) {
    const int num_threads = 4;
    const int iterations = 5;
    std::atomic<int> verify_success{0};
    std::string password = "shared_password";
    std::string hash = PasswordUtil::hash(password);

    auto worker = [&verify_success, &password, &hash]() {
        for (int i = 0; i < iterations; ++i) {
            if (PasswordUtil::verify(password, hash)) {
                verify_success++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(verify_success, num_threads * iterations);
}
