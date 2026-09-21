/* utils.h - Tiện ích bộ nhớ, chuỗi và thực thi lệnh */
#ifndef WIZK_UTILS_H
#define WIZK_UTILS_H

#include <stddef.h>
#include <sys/types.h>

// Cấp phát bộ nhớ an toàn, thoát chương trình nếu thất bại.
void *xmalloc(size_t size);

// Nhân bản chuỗi an toàn, thoát chương trình nếu thất bại.
char *xstrdup(const char *s);

// Định dạng chuỗi an toàn theo printf, trả về chuỗi mới cấp phát trên heap.
char *xasprintf(const char *fmt, ...);

// Tạo thư mục đệ quy tương đương mkdir -p, trả về 0 nếu thành công.
int mkdir_p(const char *path, mode_t mode);

// Chạy một lệnh shell và đọc toàn bộ stdout vào buffer, trả về 0 nếu thành công.
int run_cmd_output(const char *cmd, char **out, size_t *out_len);

// In thông báo lỗi ra stderr và thoát chương trình.
void die(const char *fmt, ...);

// In cảnh báo ra stderr.
void warn(const char *fmt, ...);

#endif /* WIZK_UTILS_H */
