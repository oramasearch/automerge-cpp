# Public errors

Public operations report caller-correctable failures with `std::expected<T, Error>`. An `Error`
contains a stable `ErrorKind` and a diagnostic message. `std::optional<T>` is reserved for normal
absence after the operation's inputs have been validated, such as a missing map key.

## Error kinds

| Kind | Meaning |
|---|---|
| `invalid_document` | Document data is malformed or internally inconsistent. |
| `invalid_change` | A change or requested historical head is malformed or inconsistent. |
| `invalid_obj_id` | An object identifier is not present in the document. |
| `invalid_object_type` | The object exists but does not support the requested operation. |
| `index_out_of_bounds` | A list or text index or range is outside the object. |
| `invalid_key` | A map key or mark name is not valid UTF-8. |
| `invalid_utf8` | Inserted text is not valid UTF-8. |
| `invalid_cursor` | A cursor was created for a different object. |
| `invalid_mark` | A mark range is empty, reversed, or outside its object. |
| `invalid_value_type` | A stored value has the wrong scalar type for an operation. |
| `inactive_transaction` | A transaction was used after it committed or rolled back. |
| `encoding_error` | A value could not be encoded. |
| `decoding_error` | Input bytes could not be decoded. |
| `sync_error` | A sync message or sync-state transition is invalid. |
| `invalid_operation` | An otherwise well-formed operation is invalid in its context. |

## Public operations

| Function | Failure conditions | Return type |
|---|---|---|
| `to_string_view(ErrorKind)` | None | `std::string_view` |
| `Document::transact` | The callable returns an `Error`; commit finds the transaction already inactive | `std::expected<void, Error>` or `std::expected<T, Error>` matching the callable value |
| `Document::transact_with_patches` | The callable returns an `Error`; commit finds the transaction already inactive | `std::expected<std::vector<Patch>, Error>` or `std::expected<std::pair<T, std::vector<Patch>>, Error>` matching the callable value |
| `Transaction::commit`, `rollback` | Transaction is inactive | `std::expected<void, Error>` |
| `Transaction::put`, `put_object`, `delete_key`, `put_all`, `put_map` | Inactive transaction; unknown object; non-map object; invalid UTF-8 key or string value | `std::expected<void, Error>` or `std::expected<ObjId, Error>` for object creation |
| `Transaction::insert`, `insert_object`, `insert_all`, `insert_range` | Inactive transaction; unknown object; non-sequence object; insertion index greater than length; invalid UTF-8 string value | `std::expected<void, Error>` or `std::expected<ObjId, Error>` for object creation |
| `Transaction::set`, `delete_index` | Inactive transaction; unknown object; non-sequence object; index not less than length; invalid UTF-8 string value | `std::expected<void, Error>` |
| `Transaction::splice_text` | Inactive transaction; unknown object; non-text object; invalid range; invalid UTF-8 text | `std::expected<void, Error>` |
| `Transaction::increment` | Inactive transaction; unknown object; non-map object; invalid UTF-8 key; missing or non-counter value | `std::expected<void, Error>` |
| `Transaction::mark` | Inactive transaction; unknown object; non-sequence object; empty, reversed, or out-of-range mark; empty or invalid UTF-8 name; invalid UTF-8 string value | `std::expected<void, Error>` |
| `Transaction::get(obj, key)` | Inactive transaction; unknown object; non-map object; invalid UTF-8 key | `std::expected<std::optional<Value>, Error>` |
| `Transaction::get(obj, index)` | Inactive transaction; unknown object; non-sequence object; index not less than length | `std::expected<Value, Error>` |
| `Transaction::get_obj_id` | Same object, key, and index validation as `get`; a scalar child is normal absence | `std::expected<std::optional<ObjId>, Error>` |
| `Transaction::object_type`, `length` | Inactive transaction; unknown object | `std::expected<ObjType, Error>` or `std::expected<std::size_t, Error>` |
| `Transaction::keys` | Inactive transaction; unknown object; non-map object | `std::expected<std::vector<std::string>, Error>` |
| `Transaction::text` | Inactive transaction; unknown object; non-text object | `std::expected<std::string, Error>` |
| `Document::get`, `get_all`, `operator[]` | Unknown object; non-map/list object; invalid UTF-8 key; list index not less than length | `std::expected<std::optional<Value>, Error>` or `std::expected<std::vector<Value>, Error>` |
| `Document::get<T>` | Same validation as `get`; missing map key or scalar type mismatch is normal absence | `std::expected<std::optional<T>, Error>` |
| `Document::get_path` | Wrong object or scalar type while traversing; invalid UTF-8 key; list index not less than length | `std::expected<std::optional<Value>, Error>` |
| `Document::get_obj_id` | Same object, key, and index validation as `get`; a missing or scalar child is normal absence | `std::expected<std::optional<ObjId>, Error>` |
| `Document::object_type` | Unknown object | `std::expected<ObjType, Error>` |
| `Document::keys` | Unknown object; non-map object | `std::expected<std::vector<std::string>, Error>` |
| `Document::values`, `length` | Unknown object | `std::expected<std::vector<Value>, Error>` or `std::expected<std::size_t, Error>` |
| `Document::text` | Unknown object; non-text object | `std::expected<std::string, Error>` |
| `Document::get_changes_since` | A requested head is not present in the document | `std::expected<std::vector<Change>, Error>` |
| `Document::get_at`, `keys_at`, `values_at`, `length_at`, `text_at` | Unknown historical head; then the same object, type, key, and index validation as the current-state read | Corresponding `std::expected<T, Error>` |
| `Document::cursor` | Unknown or non-sequence object; index not less than length | `std::expected<Cursor, Error>` |
| `Document::resolve_cursor` | Unknown or non-sequence object; cursor belongs to another object | `std::expected<std::optional<std::size_t>, Error>` |
| `Document::marks`, `marks_at` | Unknown or non-sequence object; `marks_at` also rejects an unknown head | `std::expected<std::vector<Mark>, Error>` |
| `Document::load`, `load_shallow` | Malformed, corrupt, truncated, or wrong-format document bytes | `std::expected<Document, Error>` |
| `SyncState::decode` | Malformed, truncated, wrong-type, or trailing bytes | `std::expected<SyncState, Error>` |
| `json::export_json`, `flatten` | Unknown subtree object | `std::expected<nlohmann::json, Error>` or `std::expected<std::map<std::string, nlohmann::json>, Error>` |
| `json::export_json_at` | Unknown subtree object or historical head | `std::expected<nlohmann::json, Error>` |
| `json::import_json` | Unknown or wrong-type target; invalid UTF-8 key or string value; inactive transaction | `std::expected<void, Error>` |
| `json::get_pointer` | Malformed pointer, invalid UTF-8 key, or out-of-range list index; missing map paths are normal absence | `std::expected<std::optional<Value>, Error>` |
| `json::put_pointer`, `delete_pointer` | Malformed or root pointer; invalid path type, key, value, or list index | `std::expected<void, Error>` |
| `json::apply_json_patch` | Malformed patch or pointer; unknown operation; missing source; failed test; invalid target key, value, or index | `std::expected<void, Error>` |
| `json::diff_json_patch`, `generate_merge_patch` | None for valid documents | `std::expected<nlohmann::json, Error>` |
| `json::apply_merge_patch` | Unknown or wrong-type target; invalid key or value | `std::expected<void, Error>` |
| `json::unflatten` | Unknown or wrong-type target; malformed pointer; invalid key or value | `std::expected<void, Error>` |
