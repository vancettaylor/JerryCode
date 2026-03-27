#include <gtest/gtest.h>
#include "cortex/core/types.hpp"

using namespace cortex;

TEST(TypesTest, ActionPlanJsonRoundTrip) {
    ActionPlan plan;
    plan.id = "test-id";
    plan.task_description = "Read a file";
    plan.tool_name = "file_read";
    plan.expected_outcome = "File contents displayed";
    plan.risk_level = "low";
    plan.files_needed = {"main.cpp"};

    Json j = plan;
    auto parsed = j.get<ActionPlan>();

    EXPECT_EQ(parsed.id, "test-id");
    EXPECT_EQ(parsed.task_description, "Read a file");
    EXPECT_EQ(parsed.tool_name, "file_read");
    EXPECT_EQ(parsed.files_needed.size(), 1);
    EXPECT_EQ(parsed.files_needed[0], "main.cpp");
}

TEST(TypesTest, TriggerKindConversion) {
    EXPECT_EQ(trigger_kind_to_string(TriggerKind::FileCreated), "file_created");
    EXPECT_EQ(trigger_kind_from_string("file_created"), TriggerKind::FileCreated);
    EXPECT_EQ(trigger_kind_to_string(TriggerKind::TaskCompleted), "task_completed");
}

TEST(TypesTest, MessageFactory) {
    auto msg = Message::user("hello");
    EXPECT_EQ(msg.role, MessageRole::User);
    EXPECT_EQ(msg.content.size(), 1);
    EXPECT_EQ(msg.content[0].text, "hello");
}
