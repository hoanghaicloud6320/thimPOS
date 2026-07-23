# 📦 THIMPOS – ORDER API CONTRACT

Base path:

```

/api/orders

````

---

# 🧱 0. DATA MODEL (CORE DESIGN)

## 🧾 orders

```sql
id           INTEGER PK
created_at   INTEGER (epoch)
updated_at   INTEGER (epoch)
````

👉 Order chỉ là container metadata

---

## 🍱 order_items

```sql
id           INTEGER PK
order_id     INTEGER FK -> orders.id
product_id   INTEGER FK -> products.id

quantity     INTEGER
unit_price   INTEGER   -- snapshot tại thời điểm tạo
line_total   INTEGER   -- quantity * unit_price

created_at   INTEGER
```

---

# 🚀 1. CREATE ORDER

## `POST /api/orders`

### Require Role

* `manager`, `staff`

### Request

```json
{
  "items": [
    { "product_id": 1, "quantity": 2 },
    { "product_id": 5, "quantity": 1 }
  ]
}
```

---

### Behavior

* BEGIN TRANSACTION
* create `orders`
* for each item:

  * validate product exists
  * fetch product.price
  * insert order_items with snapshot
* compute total = SUM(line_total)
* COMMIT

---

### Response (201)

```json id="v3h0ml"
{
  "id": 10,
  "items": [
    {
      "product_id": 1,
      "quantity": 2,
      "unit_price": 50000,
      "line_total": 100000
    },
    {
      "product_id": 5,
      "quantity": 1,
      "unit_price": 120000,
      "line_total": 120000
    }
  ],
  "total_price": 220000,
  "created_at": 1710000000
}
```

---

### Errors

```json id="8b0eqd"
{ "error": "msg" }
```

---

# 🔍 2. GET ORDER DETAIL

## `GET /api/orders/{id}`

### Require Role

* `manager`, `staff`

### Response (200)

```json id="8p2vfw"
{
  "id": 10,
  "items": [
    {
      "product_id": 1,
      "product_name": "Milk Tea",
      "quantity": 2,
      "unit_price": 50000,
      "line_total": 100000
    }
  ],
  "total_price": 100000,
  "created_at": 1710000000,
  "updated_at": 1710000000
}
```

---

### Error

#### 404

```json id="c9bb2y"
{ "error": "ORDER_NOT_FOUND" }
```

#### 500

```json id="g6r8w2"
{ "error": "catched interal error" }
```

---

# 📄 3. LIST ORDERS (SUMMARY ONLY)

## `GET /api/orders?limit=20&offset=0&from=1714000000&to=1715000000&include_items=true`

Optional query parameters:

- `from`, `to`: inclusive `created_at` epoch range.
- `include_items=true`: include product lines in each list item, avoiding one
  client request per order.
- `limit`: 1–500; `offset` must be non-negative.

### Require Role

* `manager`, `staff`

### Response

```json id="9k1yso"
{
  "items": [
    {
      "id": 10,
      "total_price": 220000,
      "created_at": 1710000000
    }
  ],
  "limit": 20,
  "offset": 0
}
```

---

Mặc định `include_items=false` để response nhẹ. Khi
`include_items=true`, mỗi phần tử có thêm mảng `items` theo cùng shape với
order detail. Response đồng thời echo `from`, `to` (nếu có) và
`include_items`.

---

# 🔄 4. REPLACE ORDER (FULL UPDATE)

## `PUT /api/orders/{id}`

> Đây là “edit order” duy nhất

---

### Require Role

* `manager`, `staff`

### Request

```json id="hzl9m2"
{
  "items": [
    { "product_id": 1, "quantity": 3 },
    { "product_id": 9, "quantity": 1 }
  ]
}
```

---

### Behavior

* BEGIN TRANSACTION
* DELETE FROM order_items WHERE order_id = ?
* INSERT lại toàn bộ items (snapshot price)
* UPDATE updated_at
* COMMIT

---

### Response

```json id="l0x3rk"
{
  "id": 10,
  "items": [...],
  "total_price": 180000,
  "updated_at": 1710001234
}
```

---

# ❌ 5. DELETE ORDER

## `DELETE /api/orders/{id}`

### Require Role

* `manager`, `staff`

### Behavior

* DELETE order_items
* DELETE orders

---

### Response

```json id="8z2k9u"
{ "success": true }
```

---

# 🧠 6. BUSINESS RULES (FINALIZED)

## 🧾 Snapshot rule (cứng)

* unit_price lấy từ products tại thời điểm tạo
* tuyệt đối không update lại theo product change

---

## 🧾 Order immutability (logic-level)

* không patch từng item
* chỉ replace full order

---

## 🧾 Total price

* cần store bắt buộc
* luôn có thể compute:

```sql
SELECT SUM(line_total)
FROM order_items
WHERE order_id = ?
```

---

## 🧾 Transaction safety

Tất cả write ops phải:

```sql
BEGIN;
...
COMMIT;
```

---

# 🧩 8. SERVICE MAPPING (DROGON COROUTINE)

| API                 | Service          |
| ------------------- | ---------------- |
| POST /orders        | createOrder()    |
| GET /orders/{id}    | getOrderDetail() |
| GET /orders         | listOrders()     |
| PUT /orders/{id}    | replaceOrder()   |
| DELETE /orders/{id} | deleteOrder()    |

---

# 🖨️ 9. PRINT BILL

## `GET /api/printbill?id={id}`

---

### 🎯 Purpose

Gửi lệnh in hóa đơn từ order đã tồn tại thông qua `TxtPrinterService`.

---

### 🔐 Require Role

* `manager`, `staff`

---

### ⚙️ Behavior

* validate order tồn tại thông qua `OrderService`
* nếu không tồn tại → 404
* nếu tồn tại:

  * build nội dung bill (từ order detail)
  * gọi `TxtPrinterService.print(...)`
  * nếu thành công → 200
  * nếu lỗi hệ thống → 500

---

### ✅ Response (200)

```json id="c1r0y5"
{
  "success": true,
  "msg": "PRINT_OK"
}
```

---

### ❌ Error Cases

#### 404 – Order not found

```json id="nq3e5c"
{
  "success": false,
  "msg": "ORDER_NOT_FOUND"
}
```

---

#### 500 – Internal error

```json id="v7cb0p"
{
  "success": false,
  "msg": "PRINT_FAILED"
}
```

---

### 🧠 Notes

* tất cả response đều phải trả JSON dạng:

```json id="c0jq0l"
{
  "success": boolean,
  "msg": string
}
```

---

### 🧩 Service Mapping

| API            | Service            |
| -------------- | ------------------ |
| GET /printbill | printBill(orderId) |

---

### 🔌 Internal Flow (concept)

```
Controller
  ↓
OrderService.getOrderDetail()
  ↓
Template render (bill.tpl)
  ↓
TxtPrinterService.print(text)
```

---

# 🔐 10. ROLE ENFORCEMENT SUMMARY

| Endpoint                | manager | staff | "" |
| ----------------------- | ------- | ----- | -- |
| POST /api/orders        | ✔       | ✔     | ✖  |
| GET /api/orders/{id}    | ✔       | ✔     | ✖  |
| GET /api/orders         | ✔       | ✔     | ✖  |
| PUT /api/orders/{id}    | ✔       | ✔     | ✖  |
| DELETE /api/orders/{id} | ✔       | ✔     | ✖  |
| GET /api/printbill      | ✔       | ✔     | ✖  |
