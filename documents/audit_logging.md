# Nhật ký hoạt động

ThimPOS ghi một dòng `LOG_INFO` dễ đọc sau mỗi thao tác API. Ví dụ:

```text
[NHẬT KÝ] admin (quản lý) xóa tài khoản staff01 — thành công.
[NHẬT KÝ] Khách xem sản phẩm số 123 — không tìm thấy.
```

Người chưa đăng nhập được hiển thị là `Khách`. Kết quả được diễn đạt bằng
ngôn ngữ thông thường như `thành công`, `chưa đăng nhập`, `không đủ quyền`
hoặc `không tìm thấy`.

Các request bị bộ lọc xác thực từ chối cũng được ghi. Dòng nhật ký chỉ chứa
thời gian và câu mô tả; không kèm tên file source, số dòng, thread ID hay các
trường HTTP kỹ thuật.

Nhật ký không ghi password, cookie, session token hay toàn bộ request body.
Các giá trị do người dùng cung cấp được giới hạn độ dài và loại bỏ ký tự xuống
dòng trước khi ghi.

Mỗi lần khởi động tạo một file mới trong `logs/`, có tên dạng
`app-recorder-YYYYMMDD-HHMMSS-PID.log`. Khi chương trình nhận yêu cầu kết
thúc, logger được flush, tháo khỏi Trantor và đóng trước khi tiến trình bỏ qua
giai đoạn destructor DLL trên Windows.
