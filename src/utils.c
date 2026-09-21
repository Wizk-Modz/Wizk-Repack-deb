/* utils.c - Triển khai tiện ích bộ nhớ, chuỗi và thực thi lệnh */
#define _GNU_SOURCE
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Cấp phát bộ nhớ an toàn, thoát chương trình nếu thất bại.
void *xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "repack: lỗi: không thể cấp phát %zu byte bộ nhớ\n", size);
        exit(1);
    }
    return p;
}

// Nhân bản chuỗi an toàn, thoát chương trình nếu thất bại.
char *xstrdup(const char *s)
{
    char *p = strdup(s);
    if (!p) {
        fprintf(stderr, "repack: lỗi: không thể nhân bản chuỗi\n");
        exit(1);
    }
    return p;
}

// Định dạng chuỗi an toàn theo printf, trả về chuỗi mới cấp phát trên heap.
char *xasprintf(const char *fmt, ...)
{
    char *str = NULL;
    va_list ap;
    va_start(ap, fmt);
    if (vasprintf(&str, fmt, ap) < 0 || !str) {
        fprintf(stderr, "repack: lỗi: không thể định dạng chuỗi\n");
        exit(1);
    }
    va_end(ap);
    return str;
}

// Tạo thư mục đệ quy tương đương mkdir -p, trả về 0 nếu thành công.
int mkdir_p(const char *path, mode_t mode)
{
    char *tmp = xstrdup(path);
    char *p = tmp;

    if (*p == '/')
        p++;

    for (; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                free(tmp);
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        free(tmp);
        return -1;
    }
    free(tmp);
    return 0;
}

// Chạy một lệnh shell và đọc toàn bộ stdout vào buffer, trả về 0 nếu thành công.
int run_cmd_output(const char *cmd, char **out, size_t *out_len)
{
    FILE *fp = popen(cmd, "r");
    if (!fp)
        return -1;

    size_t cap = 4096;
    size_t len = 0;
    char *buf = xmalloc(cap);

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, fp)) > 0) {
        len += n;
        if (len >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
            if (!buf) {
                pclose(fp);
                return -1;
            }
        }
    }

    int status = pclose(fp);
    buf[len] = '\0';
    *out = buf;
    if (out_len)
        *out_len = len;

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// In thông báo lỗi ra stderr và thoát chương trình.
void die(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "repack: lỗi: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

// In cảnh báo ra stderr.
void warn(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "repack: cảnh báo: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
