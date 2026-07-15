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
sh build.sh
```

Chỉ với bản phát triển nội bộ, có thể build mà không kiểm tra license:

```sh
cmake -S . -B build -DTHIMPOS_IGNORE_KEY_CHECK=ON
cmake --build build -j 8
```

Binary này sẽ hiển thị cảnh báo khi khởi chạy.

## Khởi chạy

ThimPOS sử dụng các đường dẫn tương đối trong `config.json`, vì vậy hãy chạy binary từ thư mục gốc của dự án. Ứng dụng tự tạo thư mục `logs` nếu thư mục này chưa tồn tại.

Không cần thiết lập biến môi trường hay sửa file cấu hình để nhập license. Ở lần chạy đầu tiên, ThimPOS sẽ tự yêu cầu nhập key trên màn hình.

Linux/macOS:

```sh
./build/ThimPOS
```

PowerShell:

```powershell
./build/ThimPOS.exe
```

Xóa cache license đã lưu và thoát mà không khởi động server:

```sh
./build/ThimPOS --clear-license-cache
```

Trên PowerShell, chạy `./build/ThimPOS.exe --clear-license-cache`.

Khi được hỏi, nhập key bản quyền đã được cấp rồi nhấn Enter. Thông báo bản quyền trên terminal hiển thị song ngữ Việt–Anh; trên Windows, chương trình tự chuyển console sang UTF-8. Sau lần kích hoạt thành công, các lần chạy tiếp theo sẽ tự kiểm tra và không yêu cầu nhập lại key. Server có thể dùng dữ liệu license đã ký khi mất kết nối đến KeyManager, miễn là thời gian sử dụng offline vẫn còn hiệu lực.

Theo cấu hình mặc định, giao diện được phục vụ tại:

```text
http://localhost/
```

Server mặc định lắng nghe cổng `80`. Có thể thay đổi địa chỉ và cổng trong `config.json`.

## Tài khoản bootstrap

Khi khởi chạy với database mới, ThimPOS tự tạo tài khoản quản lý ban đầu:

| Trường | Giá trị |
| --- | --- |
| Tên đăng nhập | `admin` |
| Mật khẩu | `mmbbmg` |
| Vai trò | `manager` |

Đăng nhập tại `http://localhost/auth/login.html`. Sau lần đăng nhập đầu tiên, hãy vào **Quản lý tài khoản nhân viên**, chọn credential `admin` và đổi mật khẩu ngay. Không tiếp tục sử dụng mật khẩu bootstrap khi triển khai thực tế.

Credential bootstrap được tạo khi database chưa có credential nào. Việc khởi động lại server không ghi đè mật khẩu đã đổi và cũng không tạo lại `admin` nếu database đã có người dùng khác.

## Lưu trữ license

- Trên Windows, license được mã hóa bằng DPAPI và lưu trong Registry của tài khoản đang sử dụng. Dữ liệu không thể chỉ copy sang máy hoặc tài khoản Windows khác để dùng lại.
- Trên Linux/macOS, license được lưu tại `~/.thimpos-license.json` với quyền chỉ chủ sở hữu đọc và ghi.

Người dùng không cần tự tạo, tìm hoặc chỉnh sửa dữ liệu lưu này. Nếu key sai, hết hạn, bị thu hồi, sai sản phẩm hoặc đạt giới hạn kích hoạt, ThimPOS sẽ hiển thị thông báo bản quyền cụ thể và không khởi chạy server.

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

SQLite mặc định sử dụng file local `csdl.sqlite3`. Các plugin tự khởi tạo bảng cần thiết khi ứng dụng chạy. Database, logs và hóa đơn đã sinh là dữ liệu runtime cục bộ, không được đưa vào Git.

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
