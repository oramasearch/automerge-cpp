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

The public-operation table is maintained below as boundary methods adopt these kinds.

## Public operations

| Function | Failure conditions | Return type |
|---|---|---|
| `to_string_view(ErrorKind)` | None | `std::string_view` |
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
