# ThimPOS Library API Contract

Domain `library` cung cấp một thư viện file dùng chung. File được lưu trực tiếp
trong `static/user_library_files` và được Drogon phục vụ như static file, không
cần endpoint download riêng.

## Phân quyền

| Thao tác | Manager | Staff | Chưa đăng nhập |
|---|---:|---:|---:|
| Xem danh sách file | Có | Có | Không |
| Upload file | Có | Có | Không |
| Xóa file | Có | Có | Không |
| Mở file bằng URL static | Có | Có | Có, nếu biết URL |

Các API quản lý chỉ dùng `AuthNFilter`. Domain không dùng `AuthZFilter`, không
phân quyền theo role và không lưu chủ sở hữu của file. Vì vậy, manager và staff
đang thao tác trên cùng một thư viện và đều có thể xóa bất kỳ file nào.

URL static là công khai theo thiết kế để file có thể được dùng làm URL trong các
domain khác và mở từ thiết bị khác.

## Data model

Metadata được đọc trực tiếp từ filesystem, không lưu trong database.

```json
{
  "name": "menu.png",
  "url": "/user_library_files/menu.png",
  "size": 24576,
  "modified_at": 1784793600
}
```

- `name`: tên file đã được server làm sạch.
- `url`: URL tương đối dùng trong cùng server.
- `size`: kích thước file tính bằng byte.
- `modified_at`: thời điểm cập nhật cuối, Unix epoch giây.

## Danh sách file

`GET /api/library/files`

Yêu cầu phiên đăng nhập hợp lệ.

### `200 OK`

```json
{
  "success": true,
  "files": [
    {
      "name": "menu.png",
      "url": "/user_library_files/menu.png",
      "size": 24576,
      "modified_at": 1784793600
    }
  ]
}
```

Danh sách được sắp xếp theo `modified_at` mới nhất trước.

## Upload file

`POST /api/library/files`

Yêu cầu phiên đăng nhập hợp lệ. Request phải là `multipart/form-data`; client có
thể gửi một hoặc nhiều phần file. Giao diện chuẩn dùng item name `files`.

Kích thước tối đa của toàn bộ HTTP request là `50M`.

### Quy tắc tên file

- Bỏ toàn bộ thành phần thư mục từ tên do client gửi lên.
- Thay các ký tự không hợp lệ trên filesystem bằng `_`.
- Không ghi đè file đã tồn tại.
- Khi trùng tên, thêm hậu tố ` (1)`, ` (2)`, ... trước phần mở rộng.
- Không chấp nhận file rỗng.

### `201 Created`

```json
{
  "success": true,
  "files": [
    {
      "name": "menu (1).png",
      "url": "/user_library_files/menu (1).png",
      "size": 24576,
      "modified_at": 1784793600
    }
  ]
}
```

## Xóa file

`DELETE /api/library/files/{name}`

Yêu cầu phiên đăng nhập hợp lệ. `name` phải được URL-encode khi chứa khoảng
trắng hoặc ký tự đặc biệt.

### `200 OK`

```json
{
  "success": true
}
```

Sau khi xóa, các domain đang tham chiếu URL của file sẽ nhận `404`.

## Truy cập file

`GET /user_library_files/{name}`

Đây là static URL do Drogon phục vụ và không đi qua `AuthNFilter`. Ví dụ URL đầy
đủ được giao diện copy:

```text
http://server-address:port/user_library_files/menu.png
```

## Lỗi

```json
{
  "success": false,
  "error": "Human-readable message"
}
```

| HTTP | Ý nghĩa |
|---:|---|
| 400 | Multipart không hợp lệ, không có file, file rỗng hoặc tên file xóa không hợp lệ. |
| 401 | Thiếu hoặc sai phiên đăng nhập khi gọi API quản lý. |
| 404 | File cần xóa không tồn tại. |
| 413 | Tổng request upload vượt giới hạn `50M` của Drogon. |
| 500 | Không thể đọc, ghi hoặc xóa file trên filesystem. |

## Giao diện

`GET /manager/files_library.html`

Giao diện hỗ trợ chọn nhiều file, kéo-thả, tìm theo tên, mở file, copy URL đầy
đủ và xóa file. Nếu API trả `401`, trình duyệt chuyển đến trang đăng nhập.
