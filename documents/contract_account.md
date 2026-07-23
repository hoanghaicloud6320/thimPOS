# HTTP Contract - Account Module

## 1. Data Models

### Account Object
| Field | Type | Description |
| :--- | :--- | :--- |
| `username` | String | Mã định danh duy nhất (Primary Key) |
| `role` | Enum | Quyền hạn: `manager`, `staff`, `` (Bắt buộc) |
| `email` | String | Địa chỉ email liên lạc |
| `phone_number` | String | Số điện thoại liên lạc |
| `full_name` | String | Họ và tên đầy đủ |
| `avatar_url` | String | Đường dẫn ảnh đại diện |
| `date_of_birth` | String | Ngày sinh (ISO8601: YYYY-MM-DD) |
| `created_at` | Timestamp | Thời điểm tạo tài khoản |
| `updated_at` | Timestamp | Có trong persistence nội bộ; API hiện không serialize field này |

### Roles Definition
- **manager**: Có toàn quyền quản lý hệ thống và quản lý các tài khoản khác.
- **staff**: Không có quyền quản lý tài khoản, chỉ có thể đăng nhập/đăng xuất.
- **""**: Người dùng chưa có role (guest-level).

---

## 2. API Endpoints & Result Structures

### 2.1 Lấy danh sách tài khoản
- **Method**: `GET`
- **Endpoint**: `/api/accounts`
- **Access**: Manager
- **Require Role**: `manager`
- **Result (Success 200 OK)**:
```json
{
  "status": "success",
  "data": [
    {
      "username": "admin_01",
      "role": "manager",
      "email": "admin@example.com",
      "phone_number": "0901234567",
      "full_name": "Nguyen Van A",
      "avatar_url": "https://link-to-image.com/img.png",
      "date_of_birth": "1990-01-01",
      "created_at": "2024-01-01T00:00:00Z"
    }
  ],
  "total": 1
}
````

---

### 2.2 Tạo tài khoản mới

* **Method**: `POST`
* **Endpoint**: `/api/accounts`
* **Access**: Manager
* **Require Role**: `manager`
* **Body**: `Account Object` (không kèm `created_at`, `updated_at`)
* **Result (Success 201 Created)**:

```json
{
  "status": "success",
  "message": "Account created successfully",
  "data": { "username": "staff_01" }
}
```

---

### 2.3 Xem chi tiết/Cập nhật/Xóa tài khoản theo username

* **Method**: `GET` | `PUT` | `DELETE`
* **Endpoint**: `/api/accounts/{username}`
* **Access**: Manager
* **Require Role**: `manager`
* **Result (Success 200 OK)**:

```json
{
  "status": "success",
  "data": {
    "username": "staff_01",
    "role": "staff",
    "email": "staff@example.com",
    "phone_number": "0907654321",
    "full_name": "Tran Thi B",
    "avatar_url": "",
    "date_of_birth": "1995-05-05"
  }
}
```

---

### 2.4 Xem/Cập nhật thông tin cá nhân

* **Method**: `GET` | `PUT`
* **Endpoint**: `/api/me`
* **Access**: Manager, Staff (readonly)
* **Require Role**:

  * `GET`: `manager`, `staff`
  * `PUT`: `manager`
* **Behavior**:

  * `manager`: full control
  * `staff`: chỉ được xem (readonly, không được sửa)
  * `""`: không được truy cập
* **Result (Success 200 OK)**: Trả về `Account Object` của người đang đăng nhập.

---

## 3. Error Responses

Mọi lỗi (400, 401, 403, 404, 500) đều trả về định dạng:

```json
{
  "status": "error",
  "code": 403,
  "message": "không đủ quyền"
}
```

---

## 4. 🔐 Role Enforcement Summary

| Endpoint                        | manager | staff | "" |
| ------------------------------- | ------- | ----- | -- |
| GET /api/accounts               | ✔       | ✖     | ✖  |
| POST /api/accounts              | ✔       | ✖     | ✖  |
| GET /api/accounts/{username}    | ✔       | ✖     | ✖  |
| PUT /api/accounts/{username}    | ✔       | ✖     | ✖  |
| DELETE /api/accounts/{username} | ✔       | ✖     | ✖  |
| GET /api/me                     | ✔       | ✔     | ✖  |
| PUT /api/me                     | ✔       | ✖     | ✖  |
