#include "appconfigdialog.h"
#include "wheelsafecombo.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QFormLayout>
#include <QTabWidget>

// 构造一个“列表 + 输入 + 添加/删除”的分组，直接操作 target 列表
static QGroupBox* makeListGroup(const QString& title, QStringList& target) {
    auto* g = new QGroupBox(title);
    auto* v = new QVBoxLayout(g);
    auto* lw = new QListWidget;
    lw->addItems(target);
    auto* le = new QLineEdit;
    le->setPlaceholderText("输入内容后点“添加”");
    auto* btnAdd = new QPushButton("添加");
    auto* btnDel = new QPushButton("删除选中");
    auto* h = new QHBoxLayout;
    h->addWidget(btnAdd);
    h->addWidget(btnDel);
    v->addWidget(lw);
    v->addWidget(le);
    v->addLayout(h);

    QObject::connect(btnAdd, &QPushButton::clicked, [le, lw, &target]() {
        QString t = le->text().trimmed();
        if (t.isEmpty()) return;
        target.append(t);
        lw->addItem(t);
        le->clear();
    });
    QObject::connect(btnDel, &QPushButton::clicked, [lw, &target]() {
        int row = lw->currentRow();
        if (row < 0) return;
        target.removeAt(row);
        delete lw->takeItem(row);
    });
    return g;
}

AppSettingsDialog::AppSettingsDialog(AppConfig& cfg, QWidget* parent) : QDialog(parent) {
    setWindowTitle("应用设置 — 外观 / 常用画风 / 环境声音 / 背景音乐");
    resize(640, 560);

    auto* v = new QVBoxLayout(this);

    // 各设置分到独立子页面，避免纵向挤压
    auto* tabs = new QTabWidget;

    // 外观
    auto* appearance = new QWidget;
    auto* av = new QVBoxLayout(appearance);
    auto* themeGroup = new QGroupBox("界面主题");
    auto* tf = new QFormLayout(themeGroup);
    auto* themeCombo = new WheelSafeComboBox;
    themeCombo->addItem("深色（默认）", "dark");
    themeCombo->addItem("浅色", "light");
    int ti = themeCombo->findData(cfg.theme);
    themeCombo->setCurrentIndex(ti < 0 ? 0 : ti);
    tf->addRow("配色风格", themeCombo);
    av->addWidget(themeGroup);
    auto* themeTip = new QLabel("切换后立即生效，并保存到应用配置，下次启动沿用。");
    themeTip->setWordWrap(true);
    av->addWidget(themeTip);
    av->addStretch(1);
    tabs->addTab(appearance, "外观");

    // 常用画风
    auto* stylePage = new QWidget;
    auto* sv = new QVBoxLayout(stylePage);
    sv->addWidget(makeListGroup("常用画风（可多条）", cfg.commonStyles));
    tabs->addTab(stylePage, "常用画风");

    // 环境声音
    auto* soundPage = new QWidget;
    auto* sov = new QVBoxLayout(soundPage);
    sov->addWidget(makeListGroup("背景环境声音 overall_soundscape（可多条）", cfg.soundscapes));
    tabs->addTab(soundPage, "环境声音");

    // 背景音乐
    auto* musicPage = new QWidget;
    auto* mv = new QVBoxLayout(musicPage);
    mv->addWidget(makeListGroup("背景音乐 non_diegetic_music（可多条，含 N/A）", cfg.musics));
    tabs->addTab(musicPage, "背景音乐");

    v->addWidget(tabs, 1);

    auto* h = new QHBoxLayout;
    auto* ok = new QPushButton("确定");
    auto* apply = new QPushButton("应用");
    auto* cancel = new QPushButton("取消");
    h->addStretch(1);
    h->addWidget(ok);
    h->addWidget(apply);
    h->addWidget(cancel);
    v->addLayout(h);

    connect(ok, &QPushButton::clicked, this, [this, &cfg, themeCombo]() {
        cfg.theme = themeCombo->currentData().toString();
        accept();
    });
    // 应用：保存配置但不关闭对话框
    connect(apply, &QPushButton::clicked, this, [this, &cfg, themeCombo]() {
        cfg.theme = themeCombo->currentData().toString();
        emit applied();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}
