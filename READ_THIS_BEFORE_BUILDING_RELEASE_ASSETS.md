# Đọc file này trước khi build release asset

Tài liệu này ghi lại quy trình build bản Windows x64 static của ThimPOS. Không dùng binary từ thư mục `build/` để phát hành và không xem việc link thành công là đủ: bản `1.0.0` vẫn chạy nhưng HTTPS tới KeyManager âm thầm thất bại do bản OpenSSL static không tự tìm thấy CA certificate.

## Mục tiêu bắt buộc

- `ThimPOS.exe` không phụ thuộc DLL của MSYS2/MinGW, Drogon, Trantor, OpenSSL, curl, SQLite hoặc JsonCpp.
- License mới phải kích hoạt được qua KeyManager cloud từ chính thư mục đóng gói.
- Gói cuối không chứa database, log, hóa đơn thử nghiệm hoặc cache license.
- ZIP phải có file SHA-256 đi kèm.

## Môi trường đã dùng cho 1.0.1

- Windows x64.
- MSYS2 UCRT64 tại `C:/msys64/ucrt64`.
- Drogon và Trantor static tại `C:/msys64/ucrt64-static`.
- CMake với Ninja.
- GitHub CLI tại `C:/Users/thuanvl/AppData/Local/CodexTools/gh/bin/gh.exe`.

Nếu đường dẫn trên thay đổi, cập nhật lệnh tương ứng. Không được bỏ các override static chỉ vì CMake tìm thấy package: mặc định nó có thể chọn file `.dll.a`.

## 1. Cấu hình build static

Chạy trong PowerShell tại root repo. Dùng một thư mục build mới cho mỗi lần phát hành:

```powershell
$curlLibs = (& pkg-config --static --libs libcurl) -join ' '

cmake -S . -B build-release-static -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++" `
  "-DCMAKE_CXX_STANDARD_LIBRARIES=$curlLibs" `
  -DDrogon_DIR=C:/msys64/ucrt64-static/lib/cmake/Drogon `
  -DTrantor_DIR=C:/msys64/ucrt64-static/lib/cmake/Trantor `
  -DOPENSSL_USE_STATIC_LIBS=TRUE `
  -DCURL_USE_STATIC_LIBS=TRUE `
  -DCURL_NO_CURL_CMAKE=TRUE `
  -DCURL_LIBRARY=C:/msys64/ucrt64/lib/libcurl.a `
  -DJSONCPP_LIBRARIES=C:/msys64/ucrt64/lib/libjsoncpp.a `
  -DSQLITE3_LIBRARIES=C:/msys64/ucrt64/lib/libsqlite3.a `
  -DZLIB_LIBRARY_RELEASE=C:/msys64/ucrt64/lib/libz.a

cmake --build build-release-static --target ThimPOS -j 8
```

### Vì sao cần từng phần

- `Drogon_DIR` và `Trantor_DIR` phải trỏ tới bản static; bản UCRT64 mặc định có thể trả về import library `.dll.a`.
- `JSONCPP_LIBRARIES`, `SQLITE3_LIBRARIES` và `ZLIB_LIBRARY_RELEASE` phải là `.a`. Trộn `.a` với `.dll.a` có thể gây lỗi duplicate symbol hoặc tạo dependency DLL ngoài ý muốn.
- `pkg-config --static --libs libcurl` bổ sung toàn bộ dependency transitive của curl như SSL, zlib, Brotli, Zstd, HTTP/2, HTTP/3, libssh2, IDN và các Windows system library. Chỉ link `libcurl.a` sẽ lỗi undefined reference.
- `CURLSSLOPT_NATIVE_CA` trong `license/KeyManagerClient.cc` là bắt buộc trên Windows. Nó cho bản curl/OpenSSL static dùng Windows trusted root store thay vì trông chờ một CA bundle bên ngoài.

## 2. Kiểm tra dependency

```powershell
C:/msys64/usr/bin/ldd.exe build-release-static/ThimPOS.exe
```

Kết quả chỉ được chứa DLL hệ thống Windows, ví dụ `KERNEL32.dll`, `CRYPT32.dll`, `WS2_32.dll`, `ADVAPI32.dll`, `USER32.dll`, `WLDAP32.dll` và các dependency hệ thống của chúng.

Không phát hành nếu thấy DLL từ `C:/msys64`, `libcurl`, `libssl`, `libcrypto`, `libdrogon`, `libtrantor`, `libstdc++`, `libgcc`, `libsqlite3` hoặc `libjsoncpp`.

## 3. Đóng gói

Tạo thư mục `dist/ThimPOS-v<version>-windows-x64/` và chỉ copy:

- `ThimPOS.exe` từ thư mục build static.
- `config.json` sạch dành cho release.
- `README.md` và hướng dẫn bắt đầu.
- `bills/activeTemplate.tpl`.
- `static/`.
- `documents/huong_dan_nguoi_dung.md` và `documents/license_check.md`.

Không copy `csdl.sqlite3`, `logs/`, bill đã in, kết quả test, cache hoặc DLL.

## 4. Test bắt buộc từ chính gói release

Mở PowerShell tại thư mục vừa đóng gói:

```powershell
./ThimPOS.exe --clear-license-cache
./ThimPOS.exe
```

Nhập một test key hợp lệ và xác nhận đủ ba điều:

1. Terminal hiển thị `License valid` từ một lần kích hoạt mới, không phải cache cũ.
2. Mở `http://localhost/` nhận HTTP `200`.
3. Chương trình chạy khi không có thư mục MSYS2 trong `PATH` hoặc trên một máy Windows sạch nếu có thể.

Sau test, dừng ThimPOS và xóa `csdl.sqlite3`, `logs/` cùng mọi dữ liệu runtime vừa sinh khỏi thư mục đóng gói. Việc xóa cache hiện không gọi `deactivate`, vì vậy cần lưu ý giới hạn phiên của test key.

## 5. Tạo ZIP và checksum

```powershell
Compress-Archive `
  -LiteralPath dist/ThimPOS-v<version>-windows-x64 `
  -DestinationPath dist/ThimPOS-v<version>-windows-x64.zip `
  -CompressionLevel Optimal

Get-FileHash `
  -Algorithm SHA256 `
  -LiteralPath dist/ThimPOS-v<version>-windows-x64.zip
```

Lưu hash vào `dist/ThimPOS-v<version>-windows-x64.zip.sha256`, sau đó tính lại và so sánh trước khi upload.

## 6. Tạo GitHub Release

Commit và push source trước, sau đó tạo/push tag trỏ đúng commit release:

```powershell
git tag -a v<version> -m "ThimPOS <version>"
git push origin v<version>

$gh = 'C:/Users/thuanvl/AppData/Local/CodexTools/gh/bin/gh.exe'
& $gh release create v<version> `
  dist/ThimPOS-v<version>-windows-x64.zip `
  dist/ThimPOS-v<version>-windows-x64.zip.sha256 `
  --repo hoanghaicloud6320/thimPOS `
  --title "ThimPOS <version>" `
  --notes-file dist/release-notes-v<version>.md `
  --verify-tag
```

Cuối cùng dùng `gh release view v<version>` để xác nhận release công khai và có đủ hai asset.

## Checklist cuối

- [ ] Build từ source/commit cần phát hành, không dùng binary cũ.
- [ ] Chỉ có Windows system DLL trong kết quả `ldd`.
- [ ] Xóa cache rồi kích hoạt cloud thành công từ binary đóng gói.
- [ ] HTTP trả về `200`.
- [ ] Đã dừng process test.
- [ ] Gói không có database/log/dữ liệu thử nghiệm/DLL.
- [ ] SHA-256 khớp.
- [ ] Tag trỏ đúng commit.
- [ ] GitHub Release có ZIP và `.sha256`.
