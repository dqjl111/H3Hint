#pragma once

#include <QWidget>
#include <QGroupBox>
#include "generator.h"

class Project;
class AppConfig;
class QListWidget;
class QComboBox;
class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QTableWidget;
class QLabel;
class QSplitter;

// 提示词实时预览面板：展示依据当前工程 + 生成选项生成的 H3 六段结构提示词，
// 文本可直接编辑；点击“更新配置”会校验局部编号（<Subject N>/<Audio N>/[Shot N]）
// 是否从 1 开始连续无缺，错误则提示且不保存，正确则存为该分镜的自定义提示词覆盖。
// 提供两种“对照”能力：① 对照模式（右侧显示结构化源数据树）；② 显示来源标注（编辑区内逐行插入灰色“// ← 来源：”注释）。
class PreviewDock : public QWidget {
    Q_OBJECT
public:
    explicit PreviewDock(Project* proj, AppConfig* cfg, int sequenceIndex = -1, QWidget* parent = nullptr);

    void refreshOptions();   // 从 AppConfig 重建选项列表
    void refresh();          // 按当前选项重新生成预览
    bool autoRefresh() const;

signals:
    // 编辑后的提示词已存为分镜自定义覆盖（MainWindow 据此标记脏并提示）
    void projectEdited();
    // 双击结构化数据表中的某行，请求跳转到对应镜头（shotIdx，相对当前分镜）的编辑页
    void requestOpenShot(int shotIdx);

private slots:
    void onOptionsChanged();
    void onTextChanged();
    void onUpdateConfig();
    void onRestoreAuto();
    void onToggleCompare(bool on);
    void onToggleAnnot(bool on);

private:
    GenOptions currentOptions() const;                 // 收集当前生成选项
    QString validateNumbering(const QString& text) const;  // 返回错误信息，空串表示通过
    QString currentCleanText() const;                  // 剔除“// ← 来源”注释后的纯提示词
    void buildSourceTable();                            // 右侧结构化源数据表

    Project* proj_;
    AppConfig* cfg_;
    int sequenceIndex_ = -1;
    bool dirty_ = false;        // 编辑器内容相对最近一次刷新/保存是否已改动
    QListWidget* lwStyle_;
    QComboBox* cbSound_;
    QComboBox* cbMusic_;
    QPlainTextEdit* editor_;
    QCheckBox* chkAuto_;
    QCheckBox* chkCompare_;
    QCheckBox* chkAnnot_;
    QTableWidget* srcTree_;
    QLabel* noteLabel_;
    QPushButton* btnUpdate_;
    QPushButton* btnRestore_;
    QGroupBox* optGroup_;        // 顶部“生成选项”分组（可折叠隐藏）
    QSplitter* optSplit_;        // 竖向分隔条：上=生成选项，下=预览/对照
    QPushButton* btnOptToggle_;  // 折叠/展开“生成选项”
};
