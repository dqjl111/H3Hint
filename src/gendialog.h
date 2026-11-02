#pragma once

#include <QDialog>
#include "project.h"

// 工程提示词生成界面：随时选用“应用内配置”的常用画风/声音/音乐，结合工程资源生成提示词
class GenDialog : public QDialog {
    Q_OBJECT
public:
    explicit GenDialog(Project& proj, AppConfig& cfg, QWidget* parent = nullptr);
};
