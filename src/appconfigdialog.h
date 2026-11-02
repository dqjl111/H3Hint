#pragma once

#include <QDialog>
#include "project.h"

// 应用内配置管理：常用画风 / 背景环境声音 / 背景音乐（每种可多条，可增删改）
class AppSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AppSettingsDialog(AppConfig& cfg, QWidget* parent = nullptr);

signals:
    // 点击「应用」时发出：请求保存当前配置（不关闭对话框）
    void applied();
};
