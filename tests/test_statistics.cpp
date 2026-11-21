#include <doctest/doctest.h>

#include "core/Statistics.h"

using namespace focus;

namespace {
SessionSummary session(std::int64_t id, SessionOutcome outcome, std::int64_t active, int aiVisits,
                       std::optional<int> budget, std::int64_t focusSec, std::int64_t aiSec) {
    SessionSummary s;
    s.id            = id;
    s.outcome       = outcome;
    s.activeSeconds = active;
    s.aiVisits      = aiVisits;
    s.aiBudget      = budget;
    s.secondsByCategory[static_cast<std::size_t>(ActivityCategory::Focus)]       = focusSec;
    s.secondsByCategory[static_cast<std::size_t>(ActivityCategory::AiAssistant)] = aiSec;
    return s;
}

CheckIn checkIn(std::int64_t sessionId, CheckInKind kind, int mood, std::string tag = "") {
    CheckIn c;
    c.sessionId = sessionId;
    c.kind      = kind;
    c.mood      = mood;
    c.tag       = std::move(tag);
    return c;
}
}  // namespace

TEST_CASE("empty input gives an empty report") {
    auto r = summarize({}, {});
    CHECK(r.sessionsCompleted == 0);
    CHECK(r.aiVisitsPerSession == 0.0);
    CHECK_FALSE(r.avgMoodChange.has_value());
}

TEST_CASE("session counts, AI usage and budget overruns") {
    std::vector<SessionSummary> s = {
        session(1, SessionOutcome::Completed, 1500, 1, 2, 1200, 300),
        session(2, SessionOutcome::Abandoned, 600, 5, 2, 300, 300),
    };
    auto r = summarize(s, {});
    CHECK(r.sessionsCompleted == 1);
    CHECK(r.sessionsAbandoned == 1);
    CHECK(r.focusSeconds == 2100);
    CHECK(r.aiVisitsPerSession == doctest::Approx(3.0));
    CHECK(r.sessionsOverAiBudget == 1);
    CHECK(r.aiTimeShare == doctest::Approx(600.0 / 2100.0));
}

TEST_CASE("mood change is measured only where both check-ins exist") {
    std::vector<CheckIn> c = {
        checkIn(1, CheckInKind::BeforeSession, 2),
        checkIn(1, CheckInKind::AfterSession, 4),   // +2
        checkIn(2, CheckInKind::BeforeSession, 3),  // no "after": excluded from change
    };
    auto r = summarize({}, c);
    CHECK(*r.avgMoodBefore == doctest::Approx(2.5));
    CHECK(*r.avgMoodAfter == doctest::Approx(4.0));
    CHECK(*r.avgMoodChange == doctest::Approx(2.0));
}

TEST_CASE("invalid check-ins are ignored and tags are counted case-insensitively") {
    std::vector<CheckIn> c = {
        checkIn(1, CheckInKind::BeforeSession, 9, "ignored"),  // mood out of range
        checkIn(1, CheckInKind::BeforeSession, 3, "Stuck"),
        checkIn(2, CheckInKind::BeforeSession, 3, "stuck"),
        checkIn(3, CheckInKind::BeforeSession, 3, "calm"),
    };
    auto r = summarize({}, c);
    REQUIRE(r.topTags.size() == 2);
    CHECK(r.topTags[0].first == "stuck");
    CHECK(r.topTags[0].second == 2);
}
