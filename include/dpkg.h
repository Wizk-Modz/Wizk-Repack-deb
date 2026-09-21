/* dpkg.h - Giao diện truy vấn metadata dpkg và sao chép maintainer scripts */
#ifndef WIZK_DPKG_H
#define WIZK_DPKG_H

#include "config.h"

// Kiểm tra gói đã cài đặt hoàn chỉnh, trả về 0 nếu OK.
int pkg_check_installed(const char *pkg, const char *root);

// Tạo tệp DEBIAN/control từ metadata của dpkg, trả về 0 nếu thành công.
int pkg_build_control(const char *pkg, const struct app_config *cfg, const char *debian_dir);

// Tạo tệp DEBIAN/conffiles từ danh sách tệp cấu hình, trả về 0 nếu thành công.
int pkg_build_conffiles(const char *pkg, const char *root, const char *debian_dir);

// Sao chép các maintainer scripts vào DEBIAN/, trả về 0 nếu thành công.
int pkg_copy_scripts(const char *pkg, const char *root, const char *debian_dir);

#endif /* WIZK_DPKG_H */
