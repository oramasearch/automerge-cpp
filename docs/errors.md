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

