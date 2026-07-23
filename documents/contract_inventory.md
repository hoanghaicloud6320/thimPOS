# Inventory API Contract

Inventory is a manager-only, ledger-based domain. Every endpoint uses
`AuthNFilter` followed by `AuthZFilter`; an authenticated account whose role is
not `manager` receives `403`.

Base path: `/api/inventory`

## Common conventions

- List endpoints accept `limit` (default `100`, range `1..500`) and `offset`
  (default `0`, non-negative).
- `from` and `to`, where supported, are inclusive Unix epoch seconds and
  `from` must not be greater than `to`.
- List responses use `{ "items": [...], "limit": n, "offset": n }` and echo
  applicable filters.
- Validation errors return `400`; an unknown update/delete target returns
  `404`; unexpected read failures return `500`.
- Domain errors use `{ "error": "Human-readable message" }`.

## Inventory item

```json
{
  "id": 1,
  "name": "Coffee beans",
  "unit": "kg",
  "is_active": true,
  "on_hand": 12.5
}
```

### `GET /api/inventory/items`

Query: `limit`, `offset`, `active_only=true|false` (default `false`).

Returns the item model above. `on_hand` is calculated from the movement ledger,
not stored as a mutable balance.

### `POST /api/inventory/items`

```json
{ "name": "Coffee beans", "unit": "kg" }
```

Success: `201 { "id": 1 }`. Name and unit must be non-empty.

### `PUT /api/inventory/items/{id}`

```json
{ "name": "Arabica beans", "unit": "kg", "is_active": true }
```

Success: `200 { "success": true }`; missing item:
`404 { "success": false }`.

## Purchase lot

```json
{
  "id": 10,
  "item_id": 1,
  "item_name": "Coffee beans",
  "quantity": 20.0,
  "remaining": 12.5,
  "unit_cost": 180000,
  "purchased_at": 1714000000,
  "expires_at": 1745536000,
  "is_void": false
}
```

### `GET /api/inventory/lots`

Query: `from`, `to`, `limit`, `offset`. The date range filters
`purchased_at`.

### `POST /api/inventory/lots`

```json
{
  "item_id": 1,
  "quantity": 20.0,
  "unit_cost": 180000,
  "purchased_at": 1714000000,
  "expires_at": 1745536000,
  "note": "April delivery"
}
```

Success: `201 { "id": 10 }`. Creating a lot also appends a positive
`purchase` movement.

### `DELETE /api/inventory/lots/{id}`

Optional JSON body:

```json
{ "note": "Damaged delivery" }
```

This operation voids the lot rather than physically deleting it and appends a
reversing `void_purchase` movement. Success:
`200 { "success": true }`.

## Product recipe

A recipe links one product to the inventory quantity consumed for a single
sold unit.

```json
{
  "id": 5,
  "product_id": 2,
  "item_id": 1,
  "item_name": "Coffee beans",
  "unit": "kg",
  "quantity": 0.018,
  "is_active": true
}
```

### `GET /api/inventory/recipes`

Query: `product_id` (optional), `active_only=true|false` (default `false`),
`limit`, `offset`.

### `POST /api/inventory/recipes`

```json
{ "product_id": 2, "item_id": 1, "quantity": 0.018 }
```

Success: `201 { "id": 5 }`.

### `PUT /api/inventory/recipes/{id}`

```json
{ "quantity": 0.02, "is_active": true }
```

Success: `200 { "success": true }`; missing recipe:
`404 { "success": false }`.

## Movement ledger

```json
{
  "id": 100,
  "item_id": 1,
  "item_name": "Coffee beans",
  "unit": "kg",
  "lot_id": 10,
  "quantity_delta": -0.036,
  "kind": "sale",
  "reference_type": "order",
  "reference_id": 24,
  "note": "",
  "created_at": 1715000000
}
```

`lot_id` and `reference_id` are omitted when absent.

### `GET /api/inventory/movements`

Query: `from`, `to`, `kind`, `limit`, `offset`.

Supported kinds:

- `purchase`: stock added by a purchase lot.
- `void_purchase`: reversal created when a lot is voided.
- `sale`: stock consumption associated with an order.
- `sale_adjustment`: delta used to synchronize inventory after an order is
  replaced or deleted.

## Order synchronization

Order writes and inventory are coupled through `InventoryService`:

- Creating an order records recipe-based consumption.
- Replacing an order records only the delta needed to match the new lines.
- Deleting an order reverses its recorded consumption.
- Product price snapshots and inventory quantities are independent; changing a
  recipe does not rewrite historic order lines or movements.

Inventory movements are the source of truth for on-hand quantity and are not
edited or deleted through the HTTP API.

## Role matrix

| Endpoint | manager | staff | no account/session |
|---|---:|---:|---:|
| All `/api/inventory/*` endpoints | Yes | No | No |
