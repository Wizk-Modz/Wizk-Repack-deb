# Wizk Repack C (`wizk-repack`)

Bản viết lại bằng ngôn ngữ C hiệu năng cao của công cụ `repack` (thay thế cho `dpkg-repack` viết bằng Perl), hỗ trợ đóng gói lại các gói `.deb` từ hệ thống tệp Debian, Ubuntu và Termux.

Maintainer: Wizk <hdshhhyd@gmail.com>

---

## 🚀 Tính năng nổi bật

- **Native C & Siêu tốc:** Không cần Perl, Python hay Shell bên ngoài. Xử lý trực tiếp metadata, lọc tệp cấu hình và diversions bằng C thuần.
- **Tuân thủ Debian Policy:**
  - Tự động chuẩn hóa quyền hạn (`0755` cho thư mục, `0644` cho tệp tin).
  - Tự động phát hiện và cấp quyền thực thi cho maintainer scripts (`preinst`, `postinst`, `prerm`, `postrm`, script có shebang `#!`).
  - Sinh tệp `DEBIAN/control` sạch, loại bỏ các trường nội bộ của dpkg (`Status:`, `Conffiles:`).
- **Hỗ trợ tệp cấu hình (`conffiles`):** Lọc bỏ các tệp cấu hình lỗi thời (`obsolete`), chỉ giữ lại các tệp thực sự tồn tại trên máy.
- **Hỗ trợ phân hướng (`dpkg-divert`):** Tự động giải quyết tệp gốc từ các quy tắc phân hướng cục bộ hoặc từ gói khác.
- **Hỗ trợ Namespace đa nền tảng:**
  - `--termux`: Namespace Termux với thư mục gốc mặc định `/data/data/com.termux/files` và tự động ánh xạ sang kiến trúc `aarch64`.
  - `--ubuntu`: Namespace Debian/Ubuntu mặc định (`/`) và kiến trúc `arm64`.
  - `--namespace=<path>`: Cho phép chỉ định thư mục gốc bất kỳ.
- **Tùy chọn Kiến trúc (Architecture):**
  - `--arch=<arch>` hoặc `-a <arch>`: Ghi đè kiến trúc xuất ra tệp deb.
  - `--aarch64`: Chuyển đổi nhanh kiến trúc thành `aarch64` (chuẩn Termux).
  - `--arm64`: Chuyển đổi nhanh kiến trúc thành `arm64` (chuẩn Debian/Ubuntu).

---

## 🛠️ Biên dịch và Cài đặt

### 1. Biên dịch thông thường
```bash
make
sudo make install PREFIX=/usr
```

### 2. Đóng gói Debian package (.deb)
```bash
dpkg-buildpackage -us -uc -b
```

### 3. Build cho Termux
```bash
./scripts/build-termux.sh
# hoặc:
make CC=clang PREFIX=/data/data/com.termux/files/usr
```

### 4. Build với Docker / GitHub Packages (WizkBuilder)
```bash
# Sử dụng base image từ ghcr.io/wizk-modz/builder
docker build -t wizk-repack .
```

---

## 💡 Hướng dẫn sử dụng

```bash
# Đóng gói cơ bản trên Ubuntu/Debian
repack nano curl

# Đóng gói với chuẩn nén XZ
repack --xz bash

# Đóng gói cho Termux (chuyển sang aarch64 và namespace Termux)
repack --termux nano

# Chỉ định rõ kiến trúc aarch64
repack --aarch64 sed

# Giữ lại thư mục tạm để kiểm tra
KEEP_TMP=1 repack sed
```
