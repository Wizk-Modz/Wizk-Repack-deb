/* fs_ops.c - Triển khai thao tác hệ thống tệp và chuẩn hóa quyền Debian */
#define _GNU_SOURCE
#include "fs_ops.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>

// Sao chép một tệp thông thường bảo toàn quyền, trả về 0 nếu thành công.
int copy_file(const char *src, const char *dst)
{
    struct stat st;
    if (lstat(src, &st) != 0)
        return -1;

    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0)
        return -1;

    int fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
    if (fd_dst < 0) {
        close(fd_src);
        return -1;
    }

    char buf[65536];
    ssize_t n;
    while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(fd_dst, buf + written, (size_t)(n - written));
            if (w < 0) {
                close(fd_src);
                close(fd_dst);
                return -1;
            }
            written += w;
        }
    }

    close(fd_src);
    close(fd_dst);

    if (n < 0)
        return -1;

    chmod(dst, st.st_mode & 07777);
    return 0;
}

// Sao chép tệp và giải phóng symlink thành regular file (tương đương cp -L).
int copy_file_deref(const char *src, const char *dst)
{
    struct stat st;
    if (stat(src, &st) != 0)
        return -1;

    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0)
        return -1;

    int fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
    if (fd_dst < 0) {
        close(fd_src);
        return -1;
    }

    char buf[65536];
    ssize_t n;
    while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(fd_dst, buf + written, (size_t)(n - written));
            if (w < 0) {
                close(fd_src);
                close(fd_dst);
                return -1;
            }
            written += w;
        }
    }

    close(fd_src);
    close(fd_dst);

    if (n < 0)
        return -1;

    chmod(dst, st.st_mode & 07777);
    return 0;
}

// Sao chép symlink nguyên bản (không đi theo link).
int copy_symlink(const char *src, const char *dst)
{
    char target[PATH_MAX];
    ssize_t len = readlink(src, target, sizeof(target) - 1);
    if (len < 0)
        return -1;
    target[len] = '\0';

    unlink(dst);
    if (symlink(target, dst) != 0)
        return -1;
    return 0;
}

// Kiểm tra tệp có bắt đầu bằng shebang (#!) hay không.
int has_shebang(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    char buf[2] = {0, 0};
    ssize_t n = read(fd, buf, 2);
    close(fd);
    return (n == 2 && buf[0] == '#' && buf[1] == '!');
}

// Đặt quyền chuẩn cho các maintainer scripts trong thư mục DEBIAN.
static int apply_debian_scripts_perms(const char *debian_dir)
{
    DIR *d = opendir(debian_dir);
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        char *fpath = xasprintf("%s/%s", debian_dir, ent->d_name);
        struct stat st;
        if (lstat(fpath, &st) != 0 || !S_ISREG(st.st_mode)) {
            free(fpath);
            continue;
        }

        const char *name = ent->d_name;
        if (strcmp(name, "preinst") == 0 || strcmp(name, "postinst") == 0 ||
            strcmp(name, "prerm") == 0 || strcmp(name, "postrm") == 0 ||
            strcmp(name, "config") == 0 || strcmp(name, "upgrade") == 0) {
            chmod(fpath, 0755);
        } else if (strcmp(name, "control") == 0 || strcmp(name, "conffiles") == 0 ||
                   strcmp(name, "md5sums") == 0 || strcmp(name, "shlibs") == 0 ||
                   strcmp(name, "symbols") == 0 || strcmp(name, "templates") == 0 ||
                   strcmp(name, "triggers") == 0) {
            chmod(fpath, 0644);
        } else {
            if (has_shebang(fpath))
                chmod(fpath, 0755);
            else
                chmod(fpath, 0644);
        }
        free(fpath);
    }
    closedir(d);
    return 0;
}

// Duyệt đệ quy cây thư mục đặt quyền 0755 cho thư mục theo Debian Policy.
static int fix_dir_perms_recursive(const char *path)
{
    DIR *d = opendir(path);
    if (!d)
        return -1;

    chmod(path, 0755);

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char *fpath = xasprintf("%s/%s", path, ent->d_name);
        struct stat st;
        if (lstat(fpath, &st) == 0 && S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode))
            fix_dir_perms_recursive(fpath);
        free(fpath);
    }
    closedir(d);
    return 0;
}

// Áp dụng quyền chuẩn Debian Policy cho thư mục build.
int apply_debian_permissions(const char *build_dir)
{
    fix_dir_perms_recursive(build_dir);

    char *debian_dir = xasprintf("%s/DEBIAN", build_dir);
    apply_debian_scripts_perms(debian_dir);
    free(debian_dir);

    return 0;
}

// Xóa đệ quy một thư mục (tương đương rm -rf).
int rmdir_recursive(const char *path)
{
    char *cmd = xasprintf("rm -rf '%s'", path);
    int rc = system(cmd);
    free(cmd);
    return rc;
}
