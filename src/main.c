/* main.c - Điều phối repack: phân tích CLI, namespace, arch và đóng gói */
#define _GNU_SOURCE
#include "config.h"
#include "dpkg.h"
#include "payload.h"
#include "fs_ops.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#define VERSION "1.0.0"
#define MAX_TMPDIRS 256

static char *g_tmpdirs[MAX_TMPDIRS];
static int g_tmpdir_count = 0;
static struct app_config g_cfg;

// Hiển thị hướng dẫn sử dụng chi tiết bằng Tiếng Việt.
static void usage(void)
{
    printf(
        "Cách dùng: repack [tùy_chọn] <gói>...\n\n"
        "Tùy chọn chung:\n"
        "  --root=<thư_mục>, -r <thư_mục>  Lấy gói từ thư mục gốc chỉ định.\n"
        "  --xz                           Nén bằng xz khi gọi dpkg-deb (-Zxz).\n"
        "  -h, --help                     Hiển thị hướng dẫn này.\n"
        "  --version                      Hiển thị phiên bản.\n\n"
        "Tùy chọn Namespace & Kiến trúc:\n"
        "  --namespace=<tên>, -n <tên>     Chỉ định: termux, ubuntu hoặc đường dẫn gốc.\n"
        "  --termux, --ubuntu             Thiết lập nhanh namespace tương ứng.\n"
        "  --arch=<kiến_trúc>, -a <arch>  Chỉ định kiến trúc xuất (aarch64, arm64...).\n"
        "  --aarch64, --arm64             Ghi đè nhanh kiến trúc xuất.\n\n"
        "Biến môi trường:\n"
        "  KEEP_TMP=1                     Giữ lại thư mục tạm để kiểm tra.\n");
}

// Hiển thị thông tin phiên bản.
static void print_version(void)
{
    printf("repack %s (Wizk <hdshhhyd@gmail.com>)\n", VERSION);
}

// Dọn dẹp toàn bộ thư mục tạm đã đăng ký khi kết thúc.
static void cleanup_all(void)
{
    if (g_cfg.keep_tmp) {
        if (g_tmpdir_count > 0) {
            fprintf(stderr, "KEEP_TMP=1, giữ lại:");
            for (int i = 0; i < g_tmpdir_count; i++)
                fprintf(stderr, " %s", g_tmpdirs[i]);
            fprintf(stderr, "\n");
        }
        config_cleanup(&g_cfg);
        return;
    }
    for (int i = 0; i < g_tmpdir_count; i++) {
        if (g_tmpdirs[i]) {
            rmdir_recursive(g_tmpdirs[i]);
            free(g_tmpdirs[i]);
            g_tmpdirs[i] = NULL;
        }
    }
    g_tmpdir_count = 0;
    config_cleanup(&g_cfg);
}

// Xử lý tín hiệu ngắt an toàn để dọn dẹp trước khi thoát.
static void signal_handler(int sig)
{
    cleanup_all();
    _exit(128 + sig);
}

// Kiểm tra các công cụ bắt buộc tồn tại trong PATH.
static void check_deps(void)
{
    const char *deps[] = {"dpkg-query", "dpkg-deb", NULL};
    for (int i = 0; deps[i]; i++) {
        char *cmd = xasprintf("command -v '%s' >/dev/null 2>&1", deps[i]);
        if (system(cmd) != 0) {
            free(cmd);
            die("không tìm thấy công cụ bắt buộc: %s", deps[i]);
        }
        free(cmd);
    }
}

// Đóng gói lại một package thành tệp deb hoàn chỉnh.
static int repack_one(const char *pkg)
{
    if (pkg_check_installed(pkg, g_cfg.root_dir) != 0)
        return -1;

    char template[256];
    snprintf(template, sizeof(template), "/tmp/%s-repack-XXXXXX", pkg);
    for (char *p = template; *p; p++) {
        if (*p == ':')
            *p = '_';
    }

    char *tmpdir = mkdtemp(template);
    if (!tmpdir) {
        warn("không thể tạo thư mục tạm cho %s", pkg);
        return -1;
    }
    chmod(tmpdir, 0755);

    if (g_tmpdir_count < MAX_TMPDIRS)
        g_tmpdirs[g_tmpdir_count++] = xstrdup(tmpdir);

    char safe_pkg[256];
    snprintf(safe_pkg, sizeof(safe_pkg), "%s", pkg);
    for (char *p = safe_pkg; *p; p++) {
        if (*p == ':')
            *p = '_';
    }

    char *build_dir = xasprintf("%s/%s-build", tmpdir, safe_pkg);
    char *debian_dir = xasprintf("%s/DEBIAN", build_dir);
    mkdir_p(debian_dir, 0755);
    chmod(build_dir, 0755);
    chmod(debian_dir, 0755);

    if (pkg_build_control(pkg, &g_cfg, debian_dir) != 0) {
        warn("không thể tạo tệp control cho %s", pkg);
        free(build_dir);
        free(debian_dir);
        return -1;
    }

    if (pkg_build_conffiles(pkg, g_cfg.root_dir, debian_dir) != 0) {
        warn("không thể tạo tệp conffiles cho %s", pkg);
        free(build_dir);
        free(debian_dir);
        return -1;
    }

    if (pkg_copy_scripts(pkg, g_cfg.root_dir, debian_dir) != 0) {
        warn("gặp lỗi khi sao chép scripts cho %s", pkg);
        free(build_dir);
        free(debian_dir);
        return -1;
    }

    if (pkg_copy_payload(pkg, g_cfg.root_dir, build_dir) != 0) {
        warn("gặp lỗi khi sao chép payload cho %s", pkg);
        free(build_dir);
        free(debian_dir);
        return -1;
    }

    apply_debian_permissions(build_dir);

    char *output = pkg_get_deb_filename(pkg, &g_cfg);
    if (!output) {
        warn("không thể xác định tên tệp deb cho %s", pkg);
        free(build_dir);
        free(debian_dir);
        return -1;
    }

    unlink(output);

    if (pkg_build_deb(build_dir, output, g_cfg.use_xz) != 0) {
        warn("dpkg-deb --build thất bại cho %s", pkg);
        free(output);
        free(build_dir);
        free(debian_dir);
        return -1;
    }

    printf("Đã tạo gói: %s\n", output);

    free(output);
    free(build_dir);
    free(debian_dir);
    return 0;
}

// Phân tích tham số dòng lệnh và thực thi repack.
int main(int argc, char *argv[])
{
    config_init(&g_cfg);
    atexit(cleanup_all);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    const char *keep_tmp_env = getenv("KEEP_TMP");
    if (keep_tmp_env && strcmp(keep_tmp_env, "1") == 0)
        g_cfg.keep_tmp = 1;

    check_deps();

    const char **pkgs = NULL;
    int pkg_count = 0;
    int pkg_cap = 16;
    pkgs = xmalloc((size_t)pkg_cap * sizeof(const char *));

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            free(pkgs);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            free(pkgs);
            return 0;
        } else if (strcmp(argv[i], "--xz") == 0) {
            g_cfg.use_xz = 1;
            i++;
        } else if (strcmp(argv[i], "--termux") == 0) {
            config_set_namespace(&g_cfg, "termux");
            i++;
        } else if (strcmp(argv[i], "--ubuntu") == 0) {
            config_set_namespace(&g_cfg, "ubuntu");
            i++;
        } else if (strcmp(argv[i], "--aarch64") == 0) {
            config_set_arch(&g_cfg, "aarch64");
            i++;
        } else if (strcmp(argv[i], "--arm64") == 0) {
            config_set_arch(&g_cfg, "arm64");
            i++;
        } else if (strncmp(argv[i], "--namespace=", 12) == 0) {
            config_set_namespace(&g_cfg, argv[i] + 12);
            i++;
        } else if ((strcmp(argv[i], "--namespace") == 0 || strcmp(argv[i], "-n") == 0) && i + 1 < argc) {
            config_set_namespace(&g_cfg, argv[++i]);
            i++;
        } else if (strncmp(argv[i], "--arch=", 7) == 0) {
            config_set_arch(&g_cfg, argv[i] + 7);
            i++;
        } else if ((strcmp(argv[i], "--arch") == 0 || strcmp(argv[i], "-a") == 0) && i + 1 < argc) {
            config_set_arch(&g_cfg, argv[++i]);
            i++;
        } else if (strncmp(argv[i], "--root=", 7) == 0) {
            free(g_cfg.root_dir);
            g_cfg.root_dir = xstrdup(argv[i] + 7);
            i++;
        } else if ((strcmp(argv[i], "--root") == 0 || strcmp(argv[i], "-r") == 0) && i + 1 < argc) {
            free(g_cfg.root_dir);
            g_cfg.root_dir = xstrdup(argv[++i]);
            i++;
        } else if (strcmp(argv[i], "--") == 0) {
            i++;
            while (i < argc) {
                if (pkg_count >= pkg_cap) {
                    pkg_cap *= 2;
                    pkgs = realloc(pkgs, (size_t)pkg_cap * sizeof(const char *));
                }
                pkgs[pkg_count++] = argv[i++];
            }
            break;
        } else if (argv[i][0] == '-') {
            usage();
            die("tùy chọn không hợp lệ: %s", argv[i]);
        } else {
            if (pkg_count >= pkg_cap) {
                pkg_cap *= 2;
                pkgs = realloc(pkgs, (size_t)pkg_cap * sizeof(const char *));
            }
            pkgs[pkg_count++] = argv[i++];
        }
    }

    if (pkg_count == 0) {
        usage();
        free(pkgs);
        die("thiếu tên gói (ví dụ: repack nano curl)");
    }

    if (g_cfg.root_dir[0] != '/')
        die("--root phải là đường dẫn tuyệt đối: %s", g_cfg.root_dir);

    int failed = 0;
    for (int j = 0; j < pkg_count; j++) {
        if (repack_one(pkgs[j]) != 0) {
            warn("gặp lỗi khi xử lý %s, đã bỏ qua", pkgs[j]);
            failed = 1;
        }
    }

    free(pkgs);
    return failed;
}
