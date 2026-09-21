/* fs_ops.h - Tiện ích sao chép tệp, symlink và chuẩn hóa quyền Debian */
#ifndef WIZK_FS_OPS_H
#define WIZK_FS_OPS_H

#include <sys/types.h>

// Sao chép một tệp thông thường bảo toàn quyền, trả về 0 nếu thành công.
int copy_file(const char *src, const char *dst);

// Sao chép tệp và giải phóng symlink thành regular file (tương đương cp -L).
int copy_file_deref(const char *src, const char *dst);

// Sao chép symlink nguyên bản (không đi theo link).
int copy_symlink(const char *src, const char *dst);

// Kiểm tra tệp có bắt đầu bằng shebang (#!) hay không.
int has_shebang(const char *path);

// Áp dụng quyền chuẩn Debian Policy cho thư mục build.
int apply_debian_permissions(const char *build_dir);

// Xóa đệ quy một thư mục (tương đương rm -rf).
int rmdir_recursive(const char *path);

#endif /* WIZK_FS_OPS_H */
