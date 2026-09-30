#include "ConfigError.h"

namespace tankcity::config {

namespace {

QString formatMessage(const QString &file, const QString &path, const QString &reason)
{
    const QString where = path.isEmpty()
        ? file
        : QStringLiteral("%1: %2").arg(file, path);
    return QStringLiteral("%1 —— %2").arg(where, reason);
}

} // namespace

ConfigError::ConfigError(const QString &file, const QString &path, const QString &reason)
    : std::runtime_error(formatMessage(file, path, reason).toStdString())
    , m_file(file)
    , m_path(path)
    , m_reason(reason)
{
}

} // namespace tankcity::config
