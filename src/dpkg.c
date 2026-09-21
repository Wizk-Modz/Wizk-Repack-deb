/* dpkg.c - Triển khai truy vấn metadata dpkg và tạo tệp control, conffiles, scripts */
#define _GNU_SOURCE
#include "dpkg.h"
#include "fs_ops.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

// Kiểm tra gói đã cài đặt hoàn chỉnh, trả về 0 nếu OK.
int pkg_check_installed(const char *pkg, const char *root)
{
    char *cmd = xasprintf("dpkg-query --root='%s' -W -f='${Status}' '%s' 2>/dev/null",
                          root, pkg);
    char *out = NULL;
    size_t out_len = 0;
    int rc = run_cmd_output(cmd, &out, &out_len);
    free(cmd);

    if (rc != 0 || !out || out_len == 0) {
        fprintf(stderr, "repack: lỗi: gói '%s' chưa được cài đặt\n", pkg);
        free(out);
        return -1;
    }

    if (strstr(out, "install ok installed") == NULL) {
        fprintf(stderr, "repack: lỗi: gói '%s' chưa được cài đặt hoàn chỉnh: %s\n",
                pkg, out);
        free(out);
        return -1;
    }

    free(out);
    return 0;
}

// Tạo tệp DEBIAN/control từ metadata của dpkg, hỗ trợ ánh xạ kiến trúc.
int pkg_build_control(const char *pkg, const struct app_config *cfg, const char *debian_dir)
{
    char *cmd = xasprintf("dpkg-query --root='%s' -s '%s' 2>/dev/null", cfg->root_dir, pkg);
    char *out = NULL;
    size_t out_len = 0;
    int rc = run_cmd_output(cmd, &out, &out_len);
    free(cmd);

    if (rc != 0 || !out || out_len == 0) {
        free(out);
        return -1;
    }

    char *control_path = xasprintf("%s/control", debian_dir);
    FILE *fp = fopen(control_path, "w");
    if (!fp) {
        free(control_path);
        free(out);
        return -1;
    }

    int in_conffiles = 0;
    char *line = out;
    char *next;

    while (line && *line) {
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }

        /* Bỏ qua dòng Status: */
        if (strncmp(line, "Status:", 7) == 0 &&
            (line[7] == ' ' || line[7] == '\t' || line[7] == '\0')) {
            line = next;
            continue;
        }

        /* Bắt đầu khối Conffiles: */
        if (strncmp(line, "Conffiles:", 10) == 0) {
            in_conffiles = 1;
            line = next;
            continue;
        }

        /* Bỏ qua các dòng tiếp nối trong khối Conffiles */
        if (in_conffiles) {
            if (line[0] == ' ' || line[0] == '\t') {
                line = next;
                continue;
            }
            in_conffiles = 0;
        }

        /* Ánh xạ trường Architecture: nếu có cấu hình hoặc namespace tương ứng */
        if (strncmp(line, "Architecture:", 13) == 0) {
            const char *val = line + 13;
            while (*val == ' ' || *val == '\t')
                val++;
            if (strcmp(val, "all") != 0) {
                char *resolved_arch = config_resolve_arch(cfg, val);
                fprintf(fp, "Architecture: %s\n", resolved_arch);
                free(resolved_arch);
                line = next;
                continue;
            }
        }

        fprintf(fp, "%s\n", line);
        line = next;
    }

    fclose(fp);
    free(out);

    struct stat st;
    if (stat(control_path, &st) != 0 || st.st_size == 0) {
        free(control_path);
        return -1;
    }

    free(control_path);
    return 0;
}

// Tạo tệp DEBIAN/conffiles từ danh sách tệp cấu hình, trả về 0 nếu thành công.
int pkg_build_conffiles(const char *pkg, const char *root, const char *debian_dir)
{
    char *cmd = xasprintf(
        "dpkg-query --root='%s' -W -f='${Conffiles}\n' '%s' 2>/dev/null",
        root, pkg);
    char *out = NULL;
    size_t out_len = 0;
    run_cmd_output(cmd, &out, &out_len);
    free(cmd);

    if (!out || out_len == 0) {
        free(out);
        return 0;
    }

    size_t cap = 64;
    size_t count = 0;
    char **files = xmalloc(cap * sizeof(char *));

    char *line = out;
    char *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }

        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0' || *p != '/') {
            line = next;
            continue;
        }

        char file_path[PATH_MAX];
        char md5[64] = "";
        char tag[64] = "";
        if (sscanf(p, "%s %s %s", file_path, md5, tag) < 1) {
            line = next;
            continue;
        }

        if (strcmp(tag, "obsolete") == 0 || strcmp(tag, "remove-on-upgrade") == 0) {
            line = next;
            continue;
        }

        char *full_path;
        if (strcmp(root, "/") == 0)
            full_path = xstrdup(file_path);
        else
            full_path = xasprintf("%s%s", root, file_path);

        struct stat st;
        if (lstat(full_path, &st) == 0) {
            if (count >= cap) {
                cap *= 2;
                files = realloc(files, cap * sizeof(char *));
            }
            files[count++] = xstrdup(file_path);
        }
        free(full_path);
        line = next;
    }

    free(out);

    if (count > 0) {
        char *conffiles_path = xasprintf("%s/conffiles", debian_dir);
        FILE *fp = fopen(conffiles_path, "w");
        if (!fp) {
            free(conffiles_path);
            for (size_t i = 0; i < count; i++)
                free(files[i]);
            free(files);
            return -1;
        }
        for (size_t i = 0; i < count; i++) {
            fprintf(fp, "%s\n", files[i]);
            free(files[i]);
        }
        fclose(fp);
        chmod(conffiles_path, 0644);
        free(conffiles_path);
    }

    free(files);
    return 0;
}

// Sao chép các maintainer scripts vào DEBIAN/, trả về 0 nếu thành công.
int pkg_copy_scripts(const char *pkg, const char *root, const char *debian_dir)
{
    char *cmd = xasprintf(
        "dpkg-query --root='%s' --control-path '%s' 2>/dev/null", root, pkg);
    char *out = NULL;
    size_t out_len = 0;
    run_cmd_output(cmd, &out, &out_len);
    free(cmd);

    if (!out || out_len == 0) {
        free(out);
        return 0;
    }

    char *line = out;
    char *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }

        if (*line == '\0') {
            line = next;
            continue;
        }

        struct stat st;
        if (lstat(line, &st) != 0) {
            warn("thiếu tệp điều khiển: %s", line);
            free(out);
            return -1;
        }

        const char *base = strrchr(line, '/');
        base = base ? base + 1 : line;
        const char *dot = strrchr(base, '.');
        const char *dest_name = (dot && dot[1] != '\0') ? dot + 1 : NULL;

        if (!dest_name || *dest_name == '\0') {
            line = next;
            continue;
        }

        if (strcmp(dest_name, "control") == 0 || strcmp(dest_name, "list") == 0) {
            line = next;
            continue;
        }

        char *dest_path = xasprintf("%s/%s", debian_dir, dest_name);

        if (copy_file_deref(line, dest_path) != 0) {
            unlink(dest_path);
            warn("bỏ qua tệp điều khiển không đọc được: %s", line);
        }

        free(dest_path);
        line = next;
    }

    free(out);
    return 0;
}
