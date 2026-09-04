#include <automerge-cpp/automerge.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

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
    const auto list = *document.transact(
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
    const auto list = *document.transact(
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
    const auto text = *document.transact(
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
    const auto text = *document.transact([](Transaction& transaction) {
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

TEST(ExpectedErrors, transaction_runner_rolls_back_a_returned_error) {
    auto document = Document{};
    const auto before = document.save();

    const auto result = document.transact([](Transaction& transaction) {
        const auto first = transaction.put(root, "temporary", 1);
        if (!first) {
            return first;
        }
        return transaction.put(unknown_object(), "key", 2);
    });

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_obj_id);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, patch_transaction_runner_rolls_back_a_returned_error) {
    auto document = Document{};
    const auto before = document.save();

    const auto result = document.transact_with_patches([](Transaction& transaction) {
        const auto first = transaction.put(root, "temporary", 1);
        if (!first) {
            return first;
        }
        return transaction.put(unknown_object(), "key", 2);
    });

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_obj_id);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, transaction_lookup_rejects_wrong_object_type_without_mutation) {
    auto document = Document{};
    const auto list = *document.transact(
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
    const auto list = *document.transact(
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

TEST(ExpectedErrors, document_lookup_rejects_unknown_object_without_mutation) {
    auto document = Document{};
    const auto before = document.save();

    const auto result = document.get(unknown_object(), "key");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_obj_id);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, document_map_lookup_rejects_sequence_object_without_mutation) {
    auto document = Document{};
    const auto list = *document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "list", ObjType::list); });
    const auto before = document.save();

    const auto result = document.get(list, "key");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_object_type);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, document_list_lookup_rejects_out_of_bounds_index_without_mutation) {
    auto document = Document{};
    const auto list = *document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "list", ObjType::list); });
    const auto before = document.save();

    const auto result = document.get(list, 0);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::index_out_of_bounds);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, document_lookup_rejects_invalid_utf8_key_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    const auto invalid_key = std::string{"\x80", 1};

    const auto result = document.get(root, invalid_key);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_key);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, document_queries_reject_wrong_object_types_without_mutation) {
    auto document = Document{};
    const auto list = *document.transact(
        [](Transaction& transaction) { return *transaction.put(root, "list", ObjType::list); });
    const auto before = document.save();

    const auto keys = document.keys(list);
    const auto text = document.text(root);

    ASSERT_FALSE(keys);
    ASSERT_FALSE(text);
    EXPECT_EQ(keys.error().kind, ErrorKind::invalid_object_type);
    EXPECT_EQ(text.error().kind, ErrorKind::invalid_object_type);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, object_type_rejects_unknown_object_without_mutation) {
    auto document = Document{};
    const auto before = document.save();

    const auto result = document.object_type(unknown_object());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_obj_id);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, path_lookup_rejects_scalar_traversal_without_mutation) {
    auto document = Document{};
    document.transact([](Transaction& transaction) { transaction.put(root, "value", 1); });
    const auto before = document.save();

    const auto result = document.get_path("value", "nested");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_value_type);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, cursors_reject_invalid_indices_and_foreign_objects_without_mutation) {
    auto document = Document{};
    const auto first = *document.transact([](Transaction& transaction) {
        const auto list = *transaction.put(root, "first", ObjType::list);
        transaction.insert(list, 0, "value");
        return list;
    });
    const auto second = *document.transact([](Transaction& transaction) {
        const auto list = *transaction.put(root, "second", ObjType::list);
        transaction.insert(list, 0, "value");
        return list;
    });
    const auto cursor = document.cursor(first, 0);
    const auto before = document.save();

    const auto out_of_bounds = document.cursor(first, 1);
    const auto foreign = document.resolve_cursor(second, *cursor);

    ASSERT_FALSE(out_of_bounds);
    ASSERT_FALSE(foreign);
    EXPECT_EQ(out_of_bounds.error().kind, ErrorKind::index_out_of_bounds);
    EXPECT_EQ(foreign.error().kind, ErrorKind::invalid_cursor);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, mark_query_rejects_map_object_without_mutation) {
    auto document = Document{};
    const auto before = document.save();

    const auto result = document.marks(root);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ErrorKind::invalid_object_type);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, historical_queries_reject_unknown_heads_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    const auto heads = std::vector<ChangeHash>{ChangeHash{}};

    const auto value = document.get_at(root, "key", heads);
    const auto changes = document.get_changes_since(heads);

    ASSERT_FALSE(value);
    ASSERT_FALSE(changes);
    EXPECT_EQ(value.error().kind, ErrorKind::invalid_change);
    EXPECT_EQ(changes.error().kind, ErrorKind::invalid_change);
    EXPECT_EQ(document.save(), before);
}

TEST(ExpectedErrors, malformed_byte_decoders_report_kinds_without_mutation) {
    auto document = Document{};
    const auto before = document.save();
    const auto bytes = std::vector<std::byte>{std::byte{0xff}};

    const auto loaded = Document::load(bytes);
    const auto shallow = Document::load_shallow(bytes);
    const auto sync_state = SyncState::decode(bytes);

    ASSERT_FALSE(loaded);
    ASSERT_FALSE(shallow);
    ASSERT_FALSE(sync_state);
    EXPECT_EQ(loaded.error().kind, ErrorKind::invalid_document);
    EXPECT_EQ(shallow.error().kind, ErrorKind::invalid_document);
    EXPECT_EQ(sync_state.error().kind, ErrorKind::decoding_error);
    EXPECT_EQ(document.save(), before);
}
