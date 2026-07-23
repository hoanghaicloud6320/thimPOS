# 🧩 ThimPOS v1.4 — Mini Spec

## 1. 🎯 Mục tiêu

Hệ POS tối giản cho shop nhỏ:

* Quản lý sản phẩm (danh mục cơ bản)
* Quản lý đơn hàng (record + in bill)
* Admin thao tác DB + web mức nhẹ

Không xử lý tồn kho, không analytics phức tạp.

---

## 2. 🧱 Kiến trúc tổng thể

### Pattern chính
* AuthNFilter
* AuthZFilter
* http/ws contnroller (gọi service để thỏa mãn http contract. Controller tự quyết định các Filter phía trước)

* **Plugin = Service (1:1)**
* Service giữ logic + repo
* Repo làm việc trực tiếp DB (SQL raw)
* Không ORM, không callback nếu coroutine dùng được

### Nguyên tắc thiết kế

* Header = contract + doc (đóng băng interface)
* Không virtual → tránh overhead + complexity
* Repo truy cập DB trực tiếp qua `app().getDbClient()`
* Plugin tự quản lý schema của mình (auto-create table)
* Tránh coupling ngang, chỉ phụ thuộc qua plugin interface

---

## 3. 🧩 Module Breakdown

### 3.1📦 Product Domain

#### ProductManagerService

* CRUD sản phẩm
* Validate dữ liệu cơ bản
* Entry point cho domain products

#### ProductRepo

* Quản lý bảng `products`
* SQL thuần
* Tự tạo bảng nếu chưa tồn tại

---

### 3.2 🧾 Order Domain

#### OrderService

* Tạo / sửa / xóa đơn
* Gắn product vào order
* Generate nội dung bill
* Gọi TxtPrinterService để in

#### OrderRepo

* Quản lý:

  * `orders`
  * `order_items`
* Mapping đơn ↔ sản phẩm (1-n)

---

### 3.3 🖨 TxtPrinter Domain

#### TxtPrinterService

* Ghi string → file `.txt`
* Gửi lệnh in (qua system command)

Cực mỏng, không chứa business logic.

### 3.4 Account Management Domain
#### AccountManagementService
* CRUD account: tạo, sửa, xóa, phân quyền
* lưu ý: ko quan tâm đến credentials
#### AccountRepo
* Quản lý bảng: `accounts`

### 3.5 AuthN (AuthN/credentials only)
#### AuthNService
* CRUD credentials
* sessions manage
#### AuthNRepo
* Quản lý bảng: `credentials`, `sessions`

### 3.6 Library Domain

#### LibraryService

* Quản lý thư viện file dùng chung trên filesystem
* Lưu file tại `static/user_library_files`
* Liệt kê metadata, upload với tên không trùng và xóa file
* Không dùng database và không lưu ownership

#### LibraryController

* API quản lý tại `/api/library/files`
* Yêu cầu đăng nhập bằng `AuthNFilter`
* Manager và staff có cùng quyền xem, upload và xóa
* URL `/user_library_files/{name}` là static URL công khai, không qua controller

#### Library UI

* File explorer tại `/manager/files_library.html`
* Hỗ trợ upload nhiều file, kéo-thả, tìm kiếm, copy link và xóa

---

## 4. 🗄 Database

### SQLite (default DB)
schema: xem trong schema.sql

---

## 5. 🔗 Dependency Graph (Plugin Level)

### Giải thích

* **OrderService**

  * cần ProductManagerService → để resolve product
  * cần TxtPrinterService → để in bill
* **ProductManagerService**

  * độc lập (leaf)
* **TxtPrinterService**

  * độc lập (leaf)
* **AccountManagementService** (leaf)
* AuthNService (leaf)
* **LibraryService** (leaf, chỉ phụ thuộc filesystem)

---

## 6. ⚙️ Plugin Config Notes

* OrderService:

  * giữ template bill (tpl file)
  * không hardcode format

## 6.1 Auth note:
* cần phân biệt rõ ràng giữa Account và Credentials, vì trong hệ thống này 2 thứ này ko sync với nhau
* AuthN chỉ để kiểm tra credentials - tức là cặp (username,password), và sessions. credentials ko phải tài khoản, nó là khóa
* AccountManagementService chỉ kiểm soát Account, ko lo về credentials

* về phần các filter:
 + AuthNFilter sẽ chỉ đơn giản là gọi AuthNService để gán `username` vào request. nếu ko có sid thì sẽ 401
 + AuthZFilter sẽ phụ thuộc vào `username` mà AuthNFilter đã gán, sau đó hỏi AccountManagementService và gán `role`. nếu ko có tài khoản thì sẽ bị 403

* **hệ quả:** ta ko nên truth AuthN. có thể đã login nhưng credential đó ko ứng với account nào cả. và ngược lại, dù có account nhưng ko có credential thì cũng ko login được
---

## 7. 🧠 Triết lý v0.1

* “Ship first, polish later”
* Tối giản abstraction nhưng giữ boundary rõ
* DB là source of truth, không layer hóa quá mức
* Service có thể tự vận hành độc lập
