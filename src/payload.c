/* payload.c - Triển khai sao chép payload, sinh tên deb và gọi dpkg-deb */
#define _GNU_SOURCE
#include "payload.h"
#include "fs_ops.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

// Sao chép toàn bộ tệp payload của gói vào thư mục build, trả về 0 nếu thành công.
int pkg_copy_payload(const char *pkg, const char *root, const char *build_dir)
{
    char *cmd = xasprintf(
        "LC_ALL=C dpkg-query --root='%s' -L '%s' 2>/dev/null", root, pkg);
    char *out = NULL;
    size_t out_len = 0;
    run_cmd_output(cmd, &out, &out_len);
    free(cmd);

    if (!out || out_len == 0) {
        free(out);
        return -1;
    }

    size_t cap = 256;
    size_t num_lines = 0;
    char **lines = xmalloc(cap * sizeof(char *));

    char *p = out;
    char *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        *nl = '\0';
        if (num_lines >= cap) {
            cap *= 2;
            lines = realloc(lines, cap * sizeof(char *));
        }
        lines[num_lines++] = p;
        p = nl + 1;
    }
    if (*p) {
        if (num_lines >= cap) {
            cap *= 2;
            lines = realloc(lines, cap * sizeof(char *));
        }
        lines[num_lines++] = p;
    }

    size_t i = 0;
    while (i < num_lines) {
        const char *orig = lines[i];
        i++;

        if (!orig || *orig == '\0')
            continue;
        if (strcmp(orig, "/.") == 0 || strcmp(orig, "/") == 0)
            continue;
        if (strstr(orig, "package diverts others to:") != NULL)
            continue;

        const char *src_path = orig;

        if (i < num_lines) {
            const char *next_line = lines[i];
            const char *divert = strstr(next_line, "locally diverted to:");
            if (!divert)
                divert = strstr(next_line, "diverted by");

            if (divert) {
                const char *colon = strstr(next_line, " to:");
                if (colon) {
                    colon += 4;
                    while (*colon == ' ')
                        colon++;
                    if (*colon)
                        src_path = colon;
                }
                i++;
            }
        }

        char *src_fs;
        if (strcmp(root, "/") == 0)
            src_fs = xstrdup(src_path);
        else
            src_fs = xasprintf("%s%s", root, src_path);

        char *dest = xasprintf("%s%s", build_dir, orig);

        struct stat st;
        if (lstat(src_fs, &st) != 0) {
            warn("bỏ qua đường dẫn không tồn tại: %s", src_fs);
            free(src_fs);
            free(dest);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            mkdir_p(dest, 0755);
            chmod(dest, st.st_mode & 07777);
        } else if (S_ISLNK(st.st_mode)) {
            char *parent = xstrdup(dest);
            char *slash = strrchr(parent, '/');
            if (slash) {
                *slash = '\0';
                mkdir_p(parent, 0755);
            }
            free(parent);
            copy_symlink(src_fs, dest);
        } else {
            char *parent = xstrdup(dest);
            char *slash = strrchr(parent, '/');
            if (slash) {
                *slash = '\0';
                mkdir_p(parent, 0755);
            }
            free(parent);
            if (copy_file(src_fs, dest) != 0) {
                warn("bỏ qua tệp không thể sao chép: %s", src_fs);
            }
        }

        free(src_fs);
        free(dest);
    }

    free(lines);
    free(out);
    return 0;
}

// Sinh tên tệp deb chuẩn, trả về chuỗi cấp phát trên heap (caller phải free).
char *pkg_get_deb_filename(const char *pkg, const struct app_config *cfg)
{
    char *cmd_pkg = xasprintf(
        "dpkg-query --root='%s' -W -f='${Package}' '%s' 2>/dev/null", cfg->root_dir, pkg);
    char *cmd_ver = xasprintf(
        "dpkg-query --root='%s' -W -f='${Version}' '%s' 2>/dev/null", cfg->root_dir, pkg);
    char *cmd_arch = xasprintf(
        "dpkg-query --root='%s' -W -f='${Architecture}' '%s' 2>/dev/null", cfg->root_dir, pkg);

    char *real_pkg = NULL, *version = NULL, *arch = NULL;
    run_cmd_output(cmd_pkg, &real_pkg, NULL);
    run_cmd_output(cmd_ver, &version, NULL);
    run_cmd_output(cmd_arch, &arch, NULL);
    free(cmd_pkg);
    free(cmd_ver);
    free(cmd_arch);

    if (!real_pkg || !*real_pkg || !version || !*version || !arch || !*arch) {
        free(real_pkg);
        free(version);
        free(arch);
        return NULL;
    }

    /* Loại bỏ epoch khỏi version */
    char *clean_ver = strchr(version, ':');
    if (clean_ver)
        clean_ver++;
    else
        clean_ver = version;

    char *nl;
    if ((nl = strchr(real_pkg, '\n')) != NULL) *nl = '\0';
    if ((nl = strchr(clean_ver, '\n')) != NULL) *nl = '\0';
    if ((nl = strchr(arch, '\n')) != NULL) *nl = '\0';

    char *resolved_arch;
    if (strcmp(arch, "all") == 0)
        resolved_arch = xstrdup("all");
    else
        resolved_arch = config_resolve_arch(cfg, arch);

    char *result = xasprintf("%s_%s_%s.deb", real_pkg, clean_ver, resolved_arch);

    free(resolved_arch);
    free(real_pkg);
    free(version);
    free(arch);
    return result;
}

// Gọi dpkg-deb để đóng gói thư mục build thành tệp deb, trả về 0 nếu thành công.
int pkg_build_deb(const char *build_dir, const char *output, int use_xz)
{
    char *cmd;
    if (use_xz)
        cmd = xasprintf("dpkg-deb --root-owner-group -Zxz --build '%s' '%s'",
                         build_dir, output);
    else
        cmd = xasprintf("dpkg-deb --root-owner-group --build '%s' '%s'",
                         build_dir, output);

    int rc = system(cmd);
    free(cmd);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}
