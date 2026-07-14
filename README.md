# ThimPOS

ThimPOS là hệ thống bán hàng gọn nhẹ dành cho cửa hàng nhỏ. Ứng dụng cung cấp giao diện tạo đơn, quản lý sản phẩm, in hóa đơn, báo cáo bán hàng và quản lý tài khoản nhân viên.

Backend được viết bằng C++20 với Drogon, sử dụng SQLite và phục vụ trực tiếp giao diện web trong thư mục `static`.

## Chức năng chính

- Tìm sản phẩm, thêm vào đơn và thanh toán trên giao diện POS.
- Tạo, xem, sửa, xóa và in đơn hàng.
- Quản lý sản phẩm, trạng thái hoạt động và hình ảnh sản phẩm.
- Xem báo cáo doanh thu theo thời gian và theo sản phẩm.
- Quản lý hồ sơ, thông tin đăng nhập, vai trò và phiên đăng nhập của nhân viên.
- Phân quyền giao diện cho `staff` và `manager`.
- Kiểm tra license đã ký trước khi khởi chạy server, kèm cache offline có thời hạn.

## Yêu cầu

- Trình biên dịch hỗ trợ C++20.
- CMake.
- Drogon.
- SQLite3.
- OpenSSL và libcurl khi build với kiểm tra license mặc định.

Các dependency cần được cài đặt sao cho CMake có thể tìm thấy bằng `find_package`.

## Build

Kiểm tra license được bật mặc định:

```sh
cmake -S . -B build
cmake --build build -j 8
```

Trên Linux/macOS cũng có thể dùng script có sẵn:

```sh
./build.sh
```

Chỉ với bản phát triển nội bộ, có thể build mà không kiểm tra license:

```sh
cmake -S . -B build -DTHIMPOS_IGNORE_KEY_CHECK=ON
cmake --build build -j 8
```

Binary này sẽ hiển thị cảnh báo khi khởi chạy.

## Khởi chạy

ThimPOS sử dụng các đường dẫn tương đối trong `config.json`, vì vậy hãy chạy binary từ thư mục gốc của dự án. Tạo thư mục log nếu chưa có.

Linux/macOS:

```sh
mkdir -p logs
THIMPOS_LICENSE_KEY='<license-key>' ./build/ThimPOS
```

PowerShell:

```powershell
New-Item -ItemType Directory -Force logs | Out-Null
$env:THIMPOS_LICENSE_KEY = '<license-key>'
./build/ThimPOS.exe
```

Sau lần kích hoạt thành công đầu tiên, client lưu license đã ký và device ID trong `.thimpos-license.json`. Server có thể dùng cache này khi mất kết nối đến KeyManager, miễn là cache vẫn còn hạn offline.

Theo cấu hình mặc định, giao diện được phục vụ tại:

```text
http://localhost/
```

Server mặc định lắng nghe cổng `80`. Có thể thay đổi địa chỉ và cổng trong `config.json`.

## Các biến môi trường license

| Biến | Mục đích |
| --- | --- |
| `THIMPOS_LICENSE_KEY` | Key dùng cho lần kích hoạt hoặc làm mới online |
| `THIMPOS_DEVICE_ID` | Device ID ổn định do hệ thống triển khai cung cấp; nếu bỏ trống, ứng dụng tự tạo |
| `THIMPOS_LICENSE_CACHE` | Thay đổi đường dẫn file cache license |
| `THIMPOS_KEY_MANAGER_URL` | Thay đổi địa chỉ KeyManager, chủ yếu dùng khi kiểm thử |

Không commit license key hoặc `.thimpos-license.json` vào Git.

## Cấu trúc dự án

```text
controllers/   HTTP controllers
filters/       Bộ lọc xác thực và phân quyền
plugins/       Service và repository của ứng dụng
license/       Client kích hoạt và xác minh license
static/        Giao diện web
documents/     Đặc tả và tài liệu hướng dẫn
test/          Kiểm thử Drogon
config.json    Cấu hình server, SQLite, static files và logging
main.cc        Điểm khởi chạy ứng dụng
```

SQLite mặc định sử dụng file `csdl.sqlite3`. Các plugin tự khởi tạo bảng cần thiết khi ứng dụng chạy.

## Tài liệu

- [Hướng dẫn người dùng cuối](documents/huong_dan_nguoi_dung.md)
- [Thiết lập và kiểm tra license](documents/license_check.md)
- [Đặc tả tổng quan](documents/spec.md)
- [Hợp đồng sản phẩm](documents/contract_products.md)
- [Hợp đồng đơn hàng](documents/contract_orders.md)
- [Hợp đồng xác thực](documents/contract_authn.md)
- [Hợp đồng tài khoản](documents/contract_account.md)

## Lưu ý triển khai

- Chỉ cấp quyền quản lý cho tài khoản cần quản trị sản phẩm, báo cáo và nhân viên.
- Sao lưu `csdl.sqlite3` định kỳ.
- Sử dụng HTTPS thông qua reverse proxy hoặc bật HTTPS trong cấu hình khi triển khai ra mạng công cộng.
- Không dùng build `THIMPOS_IGNORE_KEY_CHECK=ON` cho bản phát hành chính thức.
