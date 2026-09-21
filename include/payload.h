/* payload.h - Giao diện sao chép payload, sinh tên deb và gọi dpkg-deb */
#ifndef WIZK_PAYLOAD_H
#define WIZK_PAYLOAD_H

#include "config.h"

// Sao chép toàn bộ tệp payload của gói vào thư mục build, trả về 0 nếu thành công.
int pkg_copy_payload(const char *pkg, const char *root, const char *build_dir);

// Sinh tên tệp deb chuẩn, trả về chuỗi cấp phát trên heap (caller phải free).
char *pkg_get_deb_filename(const char *pkg, const struct app_config *cfg);

// Gọi dpkg-deb để đóng gói thư mục build thành tệp deb, trả về 0 nếu thành công.
int pkg_build_deb(const char *build_dir, const char *output, int use_xz);

#endif /* WIZK_PAYLOAD_H */
