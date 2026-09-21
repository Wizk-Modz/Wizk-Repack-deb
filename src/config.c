/* config.c - Quản lý cấu hình namespace termux/ubuntu và ánh xạ kiến trúc */
#define _GNU_SOURCE
#include "config.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

#define TERMUX_DEFAULT_ROOT "/data/data/com.termux/files"
#define UBUNTU_DEFAULT_ROOT "/"

// Khởi tạo cấu hình mặc định.
void config_init(struct app_config *cfg)
{
    cfg->ns = NS_UBUNTU;
    cfg->root_dir = xstrdup(UBUNTU_DEFAULT_ROOT);
    cfg->arch_override = NULL;
    cfg->use_xz = 0;
    cfg->keep_tmp = 0;
}

// Thiết lập namespace theo tên (termux, ubuntu) hoặc đường dẫn tùy chỉnh.
void config_set_namespace(struct app_config *cfg, const char *name_or_path)
{
    if (strcasecmp(name_or_path, "termux") == 0) {
        cfg->ns = NS_TERMUX;
        free(cfg->root_dir);
        cfg->root_dir = xstrdup(TERMUX_DEFAULT_ROOT);
    } else if (strcasecmp(name_or_path, "ubuntu") == 0 ||
               strcasecmp(name_or_path, "debian") == 0) {
        cfg->ns = NS_UBUNTU;
        free(cfg->root_dir);
        cfg->root_dir = xstrdup(UBUNTU_DEFAULT_ROOT);
    } else {
        cfg->ns = NS_CUSTOM;
        free(cfg->root_dir);
        cfg->root_dir = xstrdup(name_or_path);
    }
}

// Thiết lập kiến trúc gói mục tiêu (ví dụ: aarch64, arm64).
void config_set_arch(struct app_config *cfg, const char *arch)
{
    free(cfg->arch_override);
    cfg->arch_override = xstrdup(arch);
}

// Ánh xạ tên kiến trúc giữa các hệ thống Debian/Ubuntu và Termux.
static const char *map_arch_for_namespace(enum namespace_type ns, const char *arch)
{
    if (ns == NS_TERMUX) {
        if (strcmp(arch, "arm64") == 0)
            return "aarch64";
        if (strcmp(arch, "amd64") == 0)
            return "x86_64";
        if (strcmp(arch, "armhf") == 0)
            return "arm";
        if (strcmp(arch, "i386") == 0)
            return "i686";
    } else if (ns == NS_UBUNTU) {
        if (strcmp(arch, "aarch64") == 0)
            return "arm64";
        if (strcmp(arch, "x86_64") == 0)
            return "amd64";
        if (strcmp(arch, "arm") == 0)
            return "armhf";
        if (strcmp(arch, "i686") == 0)
            return "i386";
    }
    return arch;
}

// Xác định kiến trúc cuối cùng theo cấu hình hoặc ánh xạ namespace.
char *config_resolve_arch(const struct app_config *cfg, const char *detected_arch)
{
    if (cfg->arch_override && cfg->arch_override[0] != '\0')
        return xstrdup(cfg->arch_override);

    const char *mapped = map_arch_for_namespace(cfg->ns, detected_arch);
    return xstrdup(mapped);
}

// Giải phóng tài nguyên đã cấp phát trong cấu hình.
void config_cleanup(struct app_config *cfg)
{
    free(cfg->root_dir);
    free(cfg->arch_override);
    cfg->root_dir = NULL;
    cfg->arch_override = NULL;
}
