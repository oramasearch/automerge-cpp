#include <automerge-cpp/automerge.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace automerge_cpp;

namespace {

auto unknown_object() -> ObjId {
    return ObjId{OpId{999, ActorId{}}};
}

} // namespace

TEST(ExpectedErrors, mutator_rejects_unknown_object_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.put(unknown_object(), "key", 1);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_obj_id);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, map_mutator_rejects_list_object_without_mutation) {
    auto document = Document{};
    const auto list = document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "list", ObjType::list); });
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.put(list, "key", 1);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_object_type);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, list_mutator_rejects_out_of_bounds_index_without_mutation) {
    auto document = Document{};
    const auto list = document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "list", ObjType::list); });
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.insert(list, 1, "value");
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::index_out_of_bounds);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, mutator_rejects_invalid_utf8_key_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    const auto invalid_key = std::string{"\x80", 1};
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.put(root, invalid_key, 1);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_key);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, mutator_rejects_invalid_utf8_string_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    const auto invalid_value = std::string{"\x80", 1};
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.put(root, "key", invalid_value);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_utf8);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, compound_mutator_validates_all_entries_before_mutation) {
    auto document = Document{};
    const auto before = document.save();
    const auto invalid_key = std::string{"\x80", 1};
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.put(
            root, "container", Map{{"valid", 1}, {std::string_view{invalid_key}, 2}});
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_key);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, text_splice_rejects_out_of_bounds_range_without_mutation) {
    auto document = Document{};
    const auto text = document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "text", ObjType::text); });
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.splice_text(text, 1, 0, "x");
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::index_out_of_bounds);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, increment_rejects_non_counter_without_mutation) {
    auto document = Document{};
    document.transact([](Transaction& transaction) { transaction.put(root, "value", 1); });
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.increment(root, "value", 1);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_value_type);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, mark_rejects_invalid_range_without_mutation) {
    auto document = Document{};
    const auto text = document.transact([](Transaction& transaction) {
        const auto object = *transaction.put(root, "text", ObjType::text);
        transaction.splice_text(object, 0, 0, "x");
        return object;
    });
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.mark(text, 1, 1, "bold", true);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_mark);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, committed_transaction_rejects_further_use_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        ASSERT_TRUE(transaction.commit());
        const auto result = transaction.put(root, "key", 1);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::inactive_transaction);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, rolled_back_transaction_rejects_further_use_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        ASSERT_TRUE(transaction.rollback());
        const auto result = transaction.put(root, "key", 1);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::inactive_transaction);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, transaction_lookup_rejects_wrong_object_type_without_mutation) {
    auto document = Document{};
    const auto list = document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "list", ObjType::list); });
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.get(list, "key");
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::invalid_object_type);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, transaction_list_lookup_rejects_out_of_bounds_index_without_mutation) {
    auto document = Document{};
    const auto list = document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "list", ObjType::list); });
    const auto before = document.save();
    auto error = std::optional<Error>{};

    document.transact([&](Transaction& transaction) {
        const auto result = transaction.get(list, 0);
        ASSERT_FALSE(result);
        error = result.error();
    });

    ASSERT_TRUE(error);
    EXPECT_EQ(error->kind, ErrorKind::index_out_of_bounds);
    EXPECT_EQ(document.save(), before);
}
