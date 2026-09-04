#include <automerge-cpp/transaction.hpp>

#include "doc_state.hpp"

namespace automerge_cpp {

Transaction::Transaction(detail::DocState& state)
    : state_{state}, start_op_{state.next_counter} {}

Transaction::~Transaction() {
    rollback();
}

void Transaction::record_map_key(const ObjId& obj, const std::string& key) {
    auto* object = state_.get_object(obj);
    assert(object);

    const auto found = object->map_entries.find(key);
    if (!first_map_key_before_ && found != object->map_entries.end() && found->second.size() == 1) {
        first_map_key_before_.emplace(MapKeyBefore{
            .obj = obj,
            .key = key,
            .entries =
                MapEntryBefore{
                    .op_id = found->second.front().op_id,
                    .value = std::move(found->second.front().value),
                },
        });
        return;
    }
    auto entries =
        [&]() -> std::variant<MapKeyAbsent, MapEntryBefore, std::vector<MapEntryBefore>> {
        if (found == object->map_entries.end()) {
            return MapKeyAbsent{};
        }
        if (found->second.size() == 1) {
            return MapEntryBefore{
                .op_id = found->second.front().op_id,
                .value = found->second.front().value,
            };
        }
        auto result = std::vector<MapEntryBefore>{};
        result.reserve(found->second.size());
        std::ranges::transform(
            found->second, std::back_inserter(result), [](const detail::MapEntry& entry) {
                return MapEntryBefore{
                    .op_id = entry.op_id,
                    .value = entry.value,
                };
            });
        return result;
    }();
    auto before = MapKeyBefore{
        .obj = obj,
        .key = key,
        .entries = std::move(entries),
    };
    if (!first_map_key_before_) {
        first_map_key_before_.emplace(std::move(before));
        return;
    }
    map_keys_before_.push_back(std::move(before));
}

void Transaction::record_list_insert(const ObjId& obj, OpId element_id) {
    list_inserts_.push_back(ListInsert{
        .obj = obj,
        .element_id = element_id,
    });
}

void Transaction::record_list_value(const ObjId& obj, std::size_t index) {
    const auto* object = state_.get_object(obj);
    assert(object);
    const auto real_index = state_.visible_index_to_real(*object, index);
    assert(real_index < object->list_elements.size());
    const auto& element = object->list_elements[real_index];
    list_values_before_.push_back(ListValueBefore{
        .obj = obj,
        .element_id = element.insert_id,
        .value = element.value,
    });
}

void Transaction::record_list_visibility(const ObjId& obj, std::size_t index) {
    const auto* object = state_.get_object(obj);
    assert(object);
    const auto real_index = state_.visible_index_to_real(*object, index);
    assert(real_index < object->list_elements.size());
    const auto& element = object->list_elements[real_index];
    list_visibility_before_.push_back(ListVisibilityBefore{
        .obj = obj,
        .element_id = element.insert_id,
        .visible = element.visible,
    });
}

void Transaction::record_mark_insert(const ObjId& obj, OpId mark_id) {
    mark_inserts_.push_back(MarkInsert{
        .obj = obj,
        .mark_id = mark_id,
    });
}

void Transaction::record_object_insert(const ObjId& obj) {
    object_inserts_.push_back(ObjectInsert{.obj = obj});
}

void Transaction::begin_commit() {
    const auto clock_entry = state_.clock.find(state_.actor);
    const auto actor_clock = clock_entry == state_.clock.end()
                                 ? std::optional<std::uint64_t>{}
                                 : std::optional<std::uint64_t>{clock_entry->second};
    commit_before_.emplace(CommitBefore{
        .local_seq = state_.local_seq,
        .heads = std::move(state_.heads),
        .actor_clock = actor_clock,
        .change_history_size = state_.change_history.size(),
    });
}

void Transaction::finish_commit() {
    active_ = false;
}

void Transaction::rollback() {
    if (!active_) {
        return;
    }

    if (commit_before_) {
        state_.local_seq = commit_before_->local_seq;
        state_.heads = std::move(commit_before_->heads);
        if (commit_before_->actor_clock) {
            state_.clock[state_.actor] = *commit_before_->actor_clock;
        } else {
            state_.clock.erase(state_.actor);
        }
        state_.change_history.resize(commit_before_->change_history_size);
    }

    state_.next_counter = start_op_;

    std::ranges::for_each(mark_inserts_ | std::views::reverse, [&](const MarkInsert& inserted) {
        auto* object = state_.get_object(inserted.obj);
        if (!object) {
            return;
        }
        std::erase_if(object->marks, [&](const detail::MarkEntry& mark) {
            return mark.mark_id == inserted.mark_id;
        });
    });

    std::ranges::for_each(
        list_visibility_before_ | std::views::reverse, [&](const ListVisibilityBefore& before) {
            auto* object = state_.get_object(before.obj);
            if (!object) {
                return;
            }
            const auto found = std::ranges::find(
                object->list_elements, before.element_id, &detail::ListElement::insert_id);
            if (found != object->list_elements.end()) {
                found->visible = before.visible;
            }
        });

    std::ranges::for_each(
        list_values_before_ | std::views::reverse, [&](const ListValueBefore& before) {
            auto* object = state_.get_object(before.obj);
            if (!object) {
                return;
            }
            const auto found = std::ranges::find(
                object->list_elements, before.element_id, &detail::ListElement::insert_id);
            if (found != object->list_elements.end()) {
                found->value = before.value;
            }
        });

    std::ranges::for_each(list_inserts_ | std::views::reverse, [&](const ListInsert& inserted) {
        auto* object = state_.get_object(inserted.obj);
        if (!object) {
            return;
        }
        std::erase_if(object->list_elements, [&](const detail::ListElement& element) {
            return element.insert_id == inserted.element_id;
        });
    });

    const auto restore_map_key = [&](MapKeyBefore& before) {
        auto* object = state_.get_object(before.obj);
        if (!object) {
            return;
        }
        std::visit(overload{
                       [&](MapKeyAbsent&) { object->map_entries.erase(before.key); },
                       [&](MapEntryBefore& entry) {
                           auto restored = std::vector<detail::MapEntry>{};
                           restored.push_back(detail::MapEntry{
                               .op_id = entry.op_id,
                               .value = std::move(entry.value),
                           });
                           object->map_entries.insert_or_assign(before.key, std::move(restored));
                       },
                       [&](std::vector<MapEntryBefore>& entries) {
                           auto restored = std::vector<detail::MapEntry>{};
                           restored.reserve(entries.size());
                           std::ranges::transform(
                               entries, std::back_inserter(restored), [](MapEntryBefore& entry) {
                                   return detail::MapEntry{
                                       .op_id = entry.op_id,
                                       .value = std::move(entry.value),
                                   };
                               });
                           object->map_entries.insert_or_assign(before.key, std::move(restored));
                       },
                   },
                   before.entries);
    };
    std::ranges::for_each(map_keys_before_ | std::views::reverse, restore_map_key);
    if (first_map_key_before_) {
        restore_map_key(*first_map_key_before_);
    }

    std::ranges::for_each(object_inserts_ | std::views::reverse, [&](const ObjectInsert& inserted) {
        state_.objects.erase(inserted.obj);
    });

    pending_ops_.clear();
    active_ = false;
}

void Transaction::put(const ObjId& obj, std::string_view key, ScalarValue val) {
    assert(active_);
    auto key_str = std::string{key};
    auto pred = state_.map_pred(obj, key_str);
    record_map_key(obj, key_str);
    auto op_id = state_.next_op_id();
    auto value = Value{std::move(val)};
    state_.map_put(obj, key_str, op_id, value);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = map_key(std::move(key_str)),
        .action = OpType::put,
        .value = std::move(value),
        .pred = std::move(pred),
    };
    pending_ops_.push_back(std::move(op));
}

auto Transaction::put_object(const ObjId& obj, std::string_view key, ObjType type) -> ObjId {
    assert(active_);
    auto key_str = std::string{key};
    auto pred = state_.map_pred(obj, key_str);
    record_map_key(obj, key_str);
    auto op_id = state_.next_op_id();
    record_object_insert(ObjId{op_id});
    auto new_obj = state_.create_object(op_id, type);
    auto value = Value{type};
    state_.map_put(obj, key_str, op_id, value);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = map_key(std::move(key_str)),
        .action = OpType::make_object,
        .value = std::move(value),
        .pred = std::move(pred),
    };
    pending_ops_.push_back(std::move(op));
    return new_obj;
}

void Transaction::delete_key(const ObjId& obj, std::string_view key) {
    assert(active_);
    auto key_str = std::string{key};
    auto pred = state_.map_pred(obj, key_str);
    record_map_key(obj, key_str);
    auto op_id = state_.next_op_id();
    state_.map_delete(obj, key_str);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = map_key(std::move(key_str)),
        .action = OpType::del,
        .value = Value{ScalarValue{Null{}}},
        .pred = std::move(pred),
    };
    pending_ops_.push_back(std::move(op));
}

void Transaction::insert(const ObjId& obj, std::size_t index, ScalarValue val) {
    assert(active_);
    auto insert_after = state_.insert_after_for(obj, index);
    auto op_id = state_.next_op_id();
    record_list_insert(obj, op_id);
    auto value = Value{std::move(val)};
    state_.list_insert(obj, index, op_id, value, insert_after);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = list_index(index),
        .action = OpType::insert,
        .value = std::move(value),
        .pred = {},
        .insert_after = insert_after,
    };
    pending_ops_.push_back(std::move(op));
}

auto Transaction::insert_object(const ObjId& obj, std::size_t index, ObjType type) -> ObjId {
    assert(active_);
    auto insert_after = state_.insert_after_for(obj, index);
    auto op_id = state_.next_op_id();
    record_list_insert(obj, op_id);
    record_object_insert(ObjId{op_id});
    auto new_obj = state_.create_object(op_id, type);
    auto value = Value{type};
    state_.list_insert(obj, index, op_id, value, insert_after);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = list_index(index),
        .action = OpType::insert,
        .value = std::move(value),
        .pred = {},
        .insert_after = insert_after,
    };
    pending_ops_.push_back(std::move(op));
    return new_obj;
}

void Transaction::set(const ObjId& obj, std::size_t index, ScalarValue val) {
    assert(active_);
    auto pred = state_.list_pred(obj, index);
    record_list_value(obj, index);
    auto op_id = state_.next_op_id();
    auto value = Value{std::move(val)};
    state_.list_set(obj, index, op_id, value);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = list_index(index),
        .action = OpType::put,
        .value = std::move(value),
        .pred = std::move(pred),
    };
    pending_ops_.push_back(std::move(op));
}

void Transaction::delete_index(const ObjId& obj, std::size_t index) {
    assert(active_);
    auto pred = state_.list_pred(obj, index);
    record_list_visibility(obj, index);
    auto op_id = state_.next_op_id();
    state_.list_delete(obj, index);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = list_index(index),
        .action = OpType::del,
        .value = Value{ScalarValue{Null{}}},
        .pred = std::move(pred),
    };
    pending_ops_.push_back(std::move(op));
}

void Transaction::splice_text(const ObjId& obj, std::size_t pos, std::size_t del,
                               std::string_view text) {
    assert(active_);
    // Delete characters
    for (std::size_t i = 0; i < del; ++i) {
        auto pred = state_.list_pred(obj, pos);
        record_list_visibility(obj, pos);
        auto op_id = state_.next_op_id();
        record_list_insert(obj, op_id);
        state_.list_delete(obj, pos);
        auto op = Op{
            .id = op_id,
            .obj = obj,
            .key = list_index(pos),
            .action = OpType::del,
            .value = Value{ScalarValue{Null{}}},
            .pred = std::move(pred),
        };
        pending_ops_.push_back(std::move(op));
    }

    // Insert characters — one op per character, chaining insert_after
    auto prev_op_id = std::optional<OpId>{};
    for (std::size_t i = 0; i < text.size(); ++i) {
        auto insert_after = std::optional<OpId>{};
        if (i == 0) {
            insert_after = state_.insert_after_for(obj, pos);
        } else {
            insert_after = prev_op_id;
        }

        auto op_id = state_.next_op_id();
        prev_op_id = op_id;
        auto char_val = Value{ScalarValue{std::string(1, text[i])}};
        state_.list_insert(obj, pos + i, op_id, char_val, insert_after);
        auto op = Op{
            .id = op_id,
            .obj = obj,
            .key = list_index(pos + i),
            .action = OpType::splice_text,
            .value = std::move(char_val),
            .pred = {},
            .insert_after = insert_after,
        };
        pending_ops_.push_back(std::move(op));
    }
}

void Transaction::increment(const ObjId& obj, std::string_view key, std::int64_t delta) {
    assert(active_);
    auto key_str = std::string{key};
    auto pred = state_.map_pred(obj, key_str);
    record_map_key(obj, key_str);
    auto op_id = state_.next_op_id();
    state_.counter_increment(obj, key_str, delta);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = map_key(std::move(key_str)),
        .action = OpType::increment,
        .value = Value{ScalarValue{Counter{delta}}},
        .pred = std::move(pred),
    };
    pending_ops_.push_back(std::move(op));
}

void Transaction::mark(const ObjId& obj, std::size_t start, std::size_t end,
                       std::string_view name, ScalarValue value) {
    assert(active_);
    // Resolve start and end to element OpIds
    auto start_elem = state_.list_element_id_at(obj, start);
    assert(start_elem);
    auto end_elem = state_.list_element_id_at(obj, end > 0 ? end - 1 : 0);
    assert(end_elem);

    auto name_str = std::string{name};
    auto op_id = state_.next_op_id();
    record_mark_insert(obj, op_id);
    state_.mark_range(obj, op_id, *start_elem, *end_elem,
                      name_str, value);
    auto op = Op{
        .id = op_id,
        .obj = obj,
        .key = map_key(std::move(name_str)),
        .action = OpType::mark,
        .value = Value{value},
        .pred = {*start_elem, *end_elem},
    };
    pending_ops_.push_back(std::move(op));
}

void Transaction::commit() {
    if (!active_) {
        return;
    }
    if (pending_ops_.empty()) {
        finish_commit();
        return;
    }

    begin_commit();

    auto change = Change{
        .actor = state_.actor,
        .seq = ++state_.local_seq,
        .start_op = start_op_,
        .timestamp = 0,
        .message = std::nullopt,
        .deps = commit_before_->heads,
        .operations = std::move(pending_ops_),
    };

    auto hash = detail::DocState::compute_change_hash(change);

    // Update heads: remove deps, add new hash
    auto new_heads = std::vector<ChangeHash>{};
    for (const auto& h : commit_before_->heads) {
        if (std::ranges::find(change.deps, h) == change.deps.end()) {
            new_heads.push_back(h);
        }
    }
    new_heads.push_back(hash);
    state_.heads = std::move(new_heads);

    // Update clock
    state_.clock[state_.actor] = change.seq;

    // Store change
    state_.change_history.push_back(std::move(change));
    finish_commit();
}

// =============================================================================
// Read methods (lock-free, safe within transact's exclusive lock)
// =============================================================================

auto Transaction::get(const ObjId& obj, std::string_view key) const -> std::optional<Value> {
    return state_.map_get(obj, std::string{key});
}

auto Transaction::get(const ObjId& obj, std::size_t index) const -> std::optional<Value> {
    return state_.list_get(obj, index);
}

auto Transaction::object_type(const ObjId& obj) const -> std::optional<ObjType> {
    return state_.object_type(obj);
}

auto Transaction::get_obj_id(const ObjId& obj, std::string_view key) const -> std::optional<ObjId> {
    return state_.get_obj_id_for_key(obj, std::string{key});
}

auto Transaction::get_obj_id(const ObjId& obj, std::size_t index) const -> std::optional<ObjId> {
    return state_.get_obj_id_for_index(obj, index);
}

auto Transaction::length(const ObjId& obj) const -> std::size_t {
    return state_.object_length(obj);
}

auto Transaction::keys(const ObjId& obj) const -> std::vector<std::string> {
    return state_.map_keys(obj);
}

auto Transaction::text(const ObjId& obj) const -> std::string {
    return state_.text_content(obj);
}

}  // namespace automerge_cpp
