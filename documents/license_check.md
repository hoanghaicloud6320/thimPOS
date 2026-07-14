# Kiểm tra license khi khởi chạy

ThimPOS mặc định xác minh bản quyền trước khi mở HTTP server. Người dùng không cần thiết lập biến môi trường hoặc sửa file cấu hình.

## Chạy và kích hoạt

Khởi chạy ThimPOS như bình thường:

```text
ThimPOS chưa được kích hoạt. Vui lòng nhập key bản quyền được cấp. / ThimPOS is not activated. Please enter your license key.
License key:
```

Nhập key rồi nhấn Enter. Client sẽ kích hoạt thiết bị, kiểm tra chữ ký ES256, product `thimpos`, trạng thái, ngày bắt đầu, ngày hết hạn và cửa sổ offline. Sau khi thành công, các lần chạy sau tự sử dụng thông tin đã lưu.

## Nơi lưu license

- **Windows:** dữ liệu được mã hóa bằng Windows DPAPI và lưu tại `HKEY_CURRENT_USER\Software\ThimPOS`. Dữ liệu gắn với tài khoản Windows hiện tại nên không thể chỉ copy sang máy hoặc tài khoản khác.
- **Linux/macOS:** dữ liệu được lưu tại `~/.thimpos-license.json` với quyền file `0600`.

Người dùng không cần truy cập hoặc chỉnh sửa dữ liệu này. Nếu dữ liệu bị thay đổi, ThimPOS sẽ từ chối và yêu cầu kích hoạt lại.

## Thông báo lỗi bản quyền

Thông báo trên terminal luôn có nội dung song ngữ Việt–Anh và mã lỗi để hỗ trợ tra cứu. Trên Windows, ứng dụng tự cấu hình console sử dụng UTF-8 khi khởi chạy.

- `LICENSE_NOT_FOUND`: key không tồn tại hoặc nhập sai.
- `LICENSE_REVOKED`: key đã bị thu hồi.
- `LICENSE_EXPIRED`: bản quyền đã hết hạn.
- `LICENSE_PRODUCT_MISMATCH`: key không dành cho ThimPOS.
- `LOGIN_LIMIT_REACHED`: đã đạt giới hạn số lần kích hoạt.
- `SESSION_LIMIT_REACHED`: đã đạt giới hạn thiết bị hoặc phiên hoạt động.
- `OFFLINE_CACHE_EXPIRED`: cần kết nối Internet để làm mới quyền sử dụng offline.
- `KEY_MANAGER_UNREACHABLE`: không kết nối được máy chủ bản quyền và không có dữ liệu offline hợp lệ.

Khi lỗi bản quyền chưa được xử lý, HTTP server sẽ không khởi chạy.

## Build nội bộ không kiểm tra key

Kiểm tra key được bật mặc định:

```sh
cmake -S . -B build
```

Chỉ tắt rõ ràng cho build phát triển/nội bộ:

```sh
cmake -S . -B build -DTHIMPOS_IGNORE_KEY_CHECK=ON
```

Binary kiểu này in cảnh báo lúc khởi chạy và không chứa client verifier.
