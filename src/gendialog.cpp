#include "gendialog.h"
#include "generator.h"
#include "wheelsafecombo.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QAbstractItemView>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFile>
#include <QIODevice>

GenDialog::GenDialog(Project& proj, AppConfig& cfg, QWidget* parent) : QDialog(parent) {
    setWindowTitle("工程提示词生成界面");
    resize(860, 720);

    auto* v = new QVBoxLayout(this);

    // 工程资源摘要
    int totalShots = 0;
    for (const auto& seq : proj.sequences) totalShots += seq.shots.size();
    QString summary = QString("工程：%1　|　角色 %2　场景 %3　道具 %4　分镜 %5（镜头 %6）")
        .arg(proj.projectName).arg(proj.subjects.size()).arg(proj.scenes.size())
        .arg(proj.props.size()).arg(proj.sequences.size()).arg(totalShots);
    auto* labSum = new QLabel(summary);
    labSum->setWordWrap(true);
    v->addWidget(labSum);

    // 常用画风（多选，默认全选）
    auto* gStyle = new QGroupBox("常用画风（可多选，默认全选）");
    auto* sv = new QVBoxLayout(gStyle);
    auto* lwStyle = new QListWidget;
    lwStyle->setSelectionMode(QAbstractItemView::MultiSelection);
    for (const auto& s : cfg.commonStyles) {
        auto* it = new QListWidgetItem(s, lwStyle);
        it->setSelected(true);
    }
    sv->addWidget(lwStyle);
    v->addWidget(gStyle);

    // 背景环境声音 / 背景音乐
    auto* gSound = new QGroupBox("声音与音乐（从应用配置选取，也可手填）");
    auto* sf = new QFormLayout(gSound);
    auto* cbSound = new WheelSafeComboBox;
    cbSound->setEditable(true);
    cbSound->addItems(cfg.soundscapes);
    if (cbSound->count()) cbSound->setCurrentIndex(0);
    sf->addRow("背景环境声音 overall_soundscape", cbSound);
    auto* cbMusic = new WheelSafeComboBox;
    cbMusic->setEditable(true);
    cbMusic->addItems(cfg.musics);
    if (cbMusic->count()) cbMusic->setCurrentIndex(0);
    sf->addRow("背景音乐 non_diegetic_music", cbMusic);
    v->addWidget(gSound);

    // 工程级约束
    auto* gCon = new QGroupBox("工程约束（画风约束来自工程配置；嘴型约束在各分镜配置）");
    auto* cf = new QFormLayout(gCon);
    auto* teStyle = new QPlainTextEdit(proj.settings.styleConstraint);
    QObject::connect(teStyle, &QPlainTextEdit::textChanged, [&proj, teStyle]() {
        proj.settings.styleConstraint = teStyle->toPlainText();
    });
    cf->addRow("画风约束（工程级）", teStyle);
    v->addWidget(gCon);

    // 生成预览
    v->addWidget(new QLabel("生成预览："));
    auto* tePreview = new QPlainTextEdit;
    tePreview->setReadOnly(true);
    v->addWidget(tePreview, 1);

    // 按钮
    auto* h = new QHBoxLayout;
    auto* btnGen = new QPushButton("生成提示词");
    auto* btnCopy = new QPushButton("复制到剪贴板");
    auto* btnSave = new QPushButton("保存为 .txt");
    auto* btnClose = new QPushButton("关闭");
    h->addWidget(btnGen);
    h->addWidget(btnCopy);
    h->addWidget(btnSave);
    h->addStretch(1);
    h->addWidget(btnClose);
    v->addLayout(h);

    QObject::connect(btnGen, &QPushButton::clicked, [lwStyle, cbSound, cbMusic, tePreview, &proj, &cfg]() {
        Q_UNUSED(cfg);
        GenOptions opt;
        for (auto* it : lwStyle->selectedItems())
            opt.styles.append(it->text());
        opt.soundscape = cbSound->currentText();
        opt.music = cbMusic->currentText();
        tePreview->setPlainText(generatePrompt(proj, opt));
    });
    QObject::connect(btnCopy, &QPushButton::clicked, [tePreview]() {
        QApplication::clipboard()->setText(tePreview->toPlainText());
    });
    QObject::connect(btnSave, &QPushButton::clicked, [tePreview, &proj]() {
        QString f = QFileDialog::getSaveFileName(nullptr, "保存提示词",
                                                 proj.baseDir + "/prompt.txt", "Text (*.txt)");
        if (!f.isEmpty()) {
            QFile file(f);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(tePreview->toPlainText().toUtf8());
                file.close();
            }
        }
    });
    QObject::connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}
