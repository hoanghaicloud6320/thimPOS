# Audit logging

ThimPOS ghi một dòng `LOG_INFO` có tiền tố `[AUDIT]` sau mỗi HTTP request.
Log bao gồm tài khoản thực hiện (`actor`), vai trò, HTTP method, đường dẫn,
HTTP status, địa chỉ IP và đối tượng đăng nhập/đăng ký nếu có.

Request công khai hoặc không xác thực được ghi với
`actor=anonymous role=anonymous`. Các request bị từ chối cũng được ghi lại
với status tương ứng, ví dụ `401` hoặc `403`.

Audit log không ghi password, cookie, session token hay toàn bộ request body.
Các giá trị do người dùng cung cấp được giới hạn độ dài và loại bỏ ký tự xuống
dòng trước khi ghi.

Log được lưu chung trong `logs/app-recorder.log` và sử dụng chính sách xoay
vòng đã cấu hình cho logger của ứng dụng.
