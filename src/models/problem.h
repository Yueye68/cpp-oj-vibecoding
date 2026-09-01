#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mysql/mysql.h>
#include "connection_pool.h"

enum class Difficulty {
    Easy,
    Medium,
    Hard
};

struct Problem {
    int id = 0;
    std::string title;
    std::string description;
    Difficulty difficulty = Difficulty::Easy;
    std::vector<std::string> tags;
    int time_limit_ms = 1000;
    int memory_limit_mb = 256;
    int test_case_count = 0;
    std::string created_at;
    std::string updated_at;

    static std::string difficultyToString(Difficulty d);
    static Difficulty stringToDifficulty(const std::string& s);
    static std::string tagsToJson(const std::vector<std::string>& tags);
    static std::vector<std::string> jsonToTags(const std::string& json);

    bool create();
    bool update();
    bool remove();
    bool loadFromDB(int loadId);
    bool saveToDB();

    static std::optional<Problem> findById(int id);
    static std::vector<Problem> findAll(int page = 1, int pageSize = 20,
                                         const std::string& difficulty = "",
                                         const std::vector<std::string>& tags = {},
                                         const std::string& search = "");
    static int count(const std::string& difficulty = "",
                     const std::vector<std::string>& tags = {},
                     const std::string& search = "");
};