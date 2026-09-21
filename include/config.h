/* config.h - Định nghĩa cấu hình namespace và kiến trúc cho repack */
#ifndef WIZK_CONFIG_H
#define WIZK_CONFIG_H

enum namespace_type {
    NS_UBUNTU = 0,
    NS_TERMUX = 1,
    NS_CUSTOM = 2
};

struct app_config {
    enum namespace_type ns;
    char *root_dir;
    char *arch_override;
    int use_xz;
    int keep_tmp;
};

// Khởi tạo cấu hình mặc định.
void config_init(struct app_config *cfg);

// Thiết lập namespace theo tên (termux, ubuntu) hoặc đường dẫn tùy chỉnh.
void config_set_namespace(struct app_config *cfg, const char *name_or_path);

// Thiết lập kiến trúc gói mục tiêu (ví dụ: aarch64, arm64).
void config_set_arch(struct app_config *cfg, const char *arch);

// Xác định kiến trúc cuối cùng theo cấu hình hoặc ánh xạ namespace.
char *config_resolve_arch(const struct app_config *cfg, const char *detected_arch);

// Giải phóng tài nguyên đã cấp phát trong cấu hình.
void config_cleanup(struct app_config *cfg);

#endif /* WIZK_CONFIG_H */
