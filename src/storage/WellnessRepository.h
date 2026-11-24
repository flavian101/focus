#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "core/Wellness.h"
#include "storage/Database.h"

namespace focus {

class WellnessRepository {
public:
    explicit WellnessRepository(Database& db) : db_(db) {}

    // ---- water ---------------------------------------------------------------------------
    void addGlass(const std::string& day, int minuteOfDay, std::int64_t loggedAt);
    bool undoLastGlass(const std::string& day);  // removes today's most recent glass; false if there was none
    WaterFacts waterFor(const std::string& day);

    // ---- meals ---------------------------------------------------------------------------
    void setMeal(const std::string& day, const std::string& mealId, MealStatus status, std::int64_t loggedAt);
    void clearMeal(const std::string& day, const std::string& mealId);
    std::map<std::string, MealStatus> mealsFor(const std::string& day);

private:
    Database& db_;
};

}  // namespace focus
