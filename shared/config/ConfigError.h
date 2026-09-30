#ifndef TANKCITY_CONFIG_ERROR_H
#define TANKCITY_CONFIG_ERROR_H

#include <QString>
#include <stdexcept>

namespace tankcity::config {

/**
 * 配置加载 / 校验失败。
 *
 * 约定：what() 中包含「文件: 字段路径 —— 原因」的完整定位信息，可直接展示，
 * 例如：
 *   assets/config/entities.json: bullets.bulletAP —— 穿甲与弹射互斥（ricochet 必须为 false）
 *   assets/config/blocks.json: blocks.brik —— 未知图层 "block"（可用：background/blocks/...）
 */
class ConfigError : public std::runtime_error {
public:
    ConfigError(const QString &file, const QString &path, const QString &reason);

    const QString &file() const { return m_file; }
    const QString &path() const { return m_path; }
    const QString &reason() const { return m_reason; }

private:
    QString m_file;
    QString m_path;
    QString m_reason;
};

} // namespace tankcity::config

#endif // TANKCITY_CONFIG_ERROR_H
