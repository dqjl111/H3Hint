#include "mainwindow.h"
#include "generator.h"
#include "appconfigdialog.h"
#include "gendialog.h"
#include "previewdock.h"

#include <QDockWidget>
#include <QDialog>

#include <QTreeWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QStackedWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QStatusBar>
#include <QSignalBlocker>
#include <functional>

#include "theme.h"
#include "wheelsafecombo.h"

static void clearLayout(QLayout* lay) {
    if (!lay) return;
    while (QLayoutItem* it = lay->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
}

// 角色界面显示名：角色名 +（备注名），用于区分同名角色；不参与生成
static QString subjectDisplay(const Subject& s) {
    QString nm = s.name.isEmpty() ? QString("未命名角色") : s.name;
    if (!s.alias.isEmpty()) nm += QString("（%1）").arg(s.alias);
    return nm;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("H3Hint - MiniMax H3 提示词工程编辑器");
    resize(1100, 720);

    auto* fileMenu = menuBar()->addMenu("文件");
    auto* actNew   = fileMenu->addAction("新建工程");
    auto* actOpen  = fileMenu->addAction("打开工程...");
    recentMenu     = fileMenu->addMenu("最近打开");
    auto* actSave  = fileMenu->addAction("保存");
    auto* actSaveA = fileMenu->addAction("另存为...");
    fileMenu->addSeparator();
    auto* actGen   = fileMenu->addAction("生成提示词");
    auto* actEx    = fileMenu->addAction("载入示例工程");
    fileMenu->addSeparator();
    auto* actAbout = fileMenu->addAction("关于");
    auto* actAppCfg = fileMenu->addAction("应用设置...");

    auto* resMenu   = menuBar()->addMenu("资源");
    auto* actImp    = resMenu->addAction("导入资源...");
    resMenu->addSeparator();
    auto* actExpAll = resMenu->addAction("导出全部资源...");
    auto* actExpSub = resMenu->addAction("导出角色...");
    auto* actExpSce = resMenu->addAction("导出场景...");
    auto* actExpPro = resMenu->addAction("导出道具...");

    connect(actImp,    &QAction::triggered, this, &MainWindow::doImportAssets);
    connect(actExpAll, &QAction::triggered, this, [this] { doExportAssets(-1); });
    connect(actExpSub, &QAction::triggered, this, [this] { doExportAssets(0); });
    connect(actExpSce, &QAction::triggered, this, [this] { doExportAssets(1); });
    connect(actExpPro, &QAction::triggered, this, [this] { doExportAssets(2); });

    connect(actNew,   &QAction::triggered, this, &MainWindow::onNew);
    connect(actOpen,  &QAction::triggered, this, &MainWindow::onOpen);
    connect(actSave,  &QAction::triggered, this, &MainWindow::onSave);
    connect(actSaveA, &QAction::triggered, this, &MainWindow::onSaveAs);
    connect(actGen,   &QAction::triggered, this, &MainWindow::onGenerate);
    connect(actEx,    &QAction::triggered, this, &MainWindow::onLoadExample);
    connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);
    connect(actAppCfg, &QAction::triggered, this, &MainWindow::onAppSettings);

    auto* split = new QSplitter(Qt::Horizontal, this);
    tree = new QTreeWidget;
    tree->setHeaderLabel("工程结构");
    tree->setMinimumWidth(240);
    tree->setMaximumWidth(400);
    tree->setIconSize(QSize(16, 16));
    tree->setIndentation(14);
    tree->setAnimated(true);
    tree->setUniformRowHeights(true);
    scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    split->addWidget(tree);
    split->addWidget(scroll);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    setCentralWidget(split);

    connect(tree, &QTreeWidget::currentItemChanged, this, &MainWindow::onTreeCurrentChanged);

    // 载入应用内配置（独立于工程文件）
    appCfgPath = AppConfig::defaultPath();
    if (!appCfg.load(appCfgPath)) { appCfg.ensureDefaults(); appCfg.save(appCfgPath); }
    Theme::apply(qApp, appCfg.theme);
    refreshRecentMenu();

    onNew(); // 初始化默认工程
}

// ---------------- 树 ----------------
void MainWindow::buildTree() {
    tree->clear();
    auto* root = new QTreeWidgetItem(tree, QStringList(proj.projectName));
    root->setIcon(0, Theme::icon(Theme::Project));
    root->setData(0, Qt::UserRole, "settings");
    root->setData(0, Qt::UserRole + 1, 0);

    auto* res = new QTreeWidgetItem(root, QStringList("资源"));
    res->setIcon(0, Theme::icon(Theme::Folder));
    auto* subj = new QTreeWidgetItem(res, QStringList("角色"));
    subj->setIcon(0, Theme::icon(Theme::Folder));
    subj->setData(0, Qt::UserRole, "subjects_header");
    for (int i = 0; i < proj.subjects.size(); ++i) {
        auto* it = new QTreeWidgetItem(subj, QStringList(subjectDisplay(proj.subjects[i])));
        it->setIcon(0, Theme::icon(Theme::Character));
        it->setData(0, Qt::UserRole, "subject");
        it->setData(0, Qt::UserRole + 1, i);
    }
    auto* sc = new QTreeWidgetItem(res, QStringList("场景"));
    sc->setIcon(0, Theme::icon(Theme::Folder));
    sc->setData(0, Qt::UserRole, "scenes_header");
    for (int i = 0; i < proj.scenes.size(); ++i) {
        QString ln = proj.scenes[i].label.trimmed().isEmpty()
                         ? QString("场景%1").arg(i + 1) : proj.scenes[i].label;
        auto* it = new QTreeWidgetItem(sc, QStringList(ln));
        it->setIcon(0, Theme::icon(Theme::Scene));
        it->setData(0, Qt::UserRole, "scene");
        it->setData(0, Qt::UserRole + 1, i);
    }
    auto* pr = new QTreeWidgetItem(res, QStringList("道具"));
    pr->setIcon(0, Theme::icon(Theme::Folder));
    pr->setData(0, Qt::UserRole, "props_header");
    for (int i = 0; i < proj.props.size(); ++i) {
        QString ln = proj.props[i].label.trimmed().isEmpty()
                         ? QString("道具%1").arg(i + 1) : proj.props[i].label;
        auto* it = new QTreeWidgetItem(pr, QStringList(ln));
        it->setIcon(0, Theme::icon(Theme::Prop));
        it->setData(0, Qt::UserRole, "prop");
        it->setData(0, Qt::UserRole + 1, i);
    }
    auto* seqs = new QTreeWidgetItem(root, QStringList("分镜"));
    seqs->setIcon(0, Theme::icon(Theme::Folder));
    seqs->setData(0, Qt::UserRole, "sequences_header");
    for (int i = 0; i < proj.sequences.size(); ++i) {
        const auto& seq = proj.sequences[i];
        QString nm = seq.name.trimmed().isEmpty() ? QString("分镜%1").arg(i + 1) : seq.name;
        auto* sit = new QTreeWidgetItem(seqs, QStringList(QString("%1. %2").arg(i + 1).arg(nm)));
        sit->setIcon(0, Theme::icon(Theme::Sequence));
        sit->setData(0, Qt::UserRole, "sequence");
        sit->setData(0, Qt::UserRole + 1, i);
        for (int j = 0; j < seq.shots.size(); ++j) {
            QString sn = seq.shots[j].label.trimmed().isEmpty()
                             ? QString("镜头%1").arg(j + 1) : seq.shots[j].label;
            auto* jit = new QTreeWidgetItem(sit, QStringList(sn));
            jit->setIcon(0, Theme::icon(Theme::Shot));
            jit->setData(0, Qt::UserRole, "shot");
            jit->setData(0, Qt::UserRole + 1, j);       // shotIdx
            jit->setData(0, Qt::UserRole + 2, i);        // seqIdx
        }
    }
    tree->expandAll();
}

void MainWindow::selectItem(const QString& kind, int idx) {
    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> find =
        [&](QTreeWidgetItem* parent) -> QTreeWidgetItem* {
            for (int i = 0; i < parent->childCount(); ++i) {
                auto* c = parent->child(i);
                if (c->data(0, Qt::UserRole).toString() == kind &&
                    (idx < 0 || c->data(0, Qt::UserRole + 1).toInt() == idx))
                    return c;
                auto* r = find(c);
                if (r) return r;
            }
            return nullptr;
        };
    QTreeWidgetItem* it = find(tree->invisibleRootItem());
    if (it) tree->setCurrentItem(it);
}

void MainWindow::updateCurrentLabel() {
    auto* it = tree->currentItem();
    if (!it) return;
    QString kind = it->data(0, Qt::UserRole).toString();
    int idx = it->data(0, Qt::UserRole + 1).toInt();
    if (kind == "subject")      it->setText(0, subjectDisplay(proj.subjects[idx]));
    else if (kind == "scene")   it->setText(0, proj.scenes[idx].label.trimmed().isEmpty() ? QString("场景%1").arg(idx + 1) : proj.scenes[idx].label);
    else if (kind == "prop")    it->setText(0, proj.props[idx].label.trimmed().isEmpty() ? QString("道具%1").arg(idx + 1) : proj.props[idx].label);
    else if (kind == "sequence") {
        QString nm = proj.sequences[idx].name.trimmed().isEmpty()
                         ? QString("分镜%1").arg(idx + 1) : proj.sequences[idx].name;
        it->setText(0, QString("%1. %2").arg(idx + 1).arg(nm));
    }
    else if (kind == "shot") {
        int seqIdx = it->data(0, Qt::UserRole + 2).toInt();
        int shotIdx = idx;
        const auto& sh = proj.sequences[seqIdx].shots[shotIdx];
        it->setText(0, sh.label.trimmed().isEmpty() ? QString("镜头%1").arg(shotIdx + 1) : sh.label);
    }
    else if (kind == "settings")it->setText(0, proj.projectName);
}

// ---------------- 编辑器切换 ----------------
void MainWindow::setEditor(QWidget* w) {
    QWidget* old = scroll->takeWidget();
    scroll->setWidget(w);
    if (old) old->deleteLater();
}

void MainWindow::onTreeCurrentChanged(QTreeWidgetItem* cur, QTreeWidgetItem*) {
    if (!cur) return;
    buildEditor(cur);
}

void MainWindow::buildEditor(QTreeWidgetItem* item) {
    QString kind = item->data(0, Qt::UserRole).toString();
    int idx = item->data(0, Qt::UserRole + 1).toInt();
    QWidget* w = nullptr;
    if (kind == "settings")         w = buildSettingsEditor();
    else if (kind == "subjects_header") w = buildSubjectsHeader();
    else if (kind == "subject")     w = buildSubjectEditor(idx);
    else if (kind == "scenes_header")  w = buildScenesHeader();
    else if (kind == "scene")       w = buildSceneEditor(idx);
    else if (kind == "props_header")   w = buildPropsHeader();
    else if (kind == "prop")        w = buildPropEditor(idx);
    else if (kind == "sequences_header") w = buildSequencesHeader();
    else if (kind == "sequence")     w = buildSequenceEditor(idx);
    else if (kind == "shot") {
        int seqIdx = item->data(0, Qt::UserRole + 2).toInt();
        w = buildShotEditor(seqIdx, idx);
    }
    if (w) setEditor(w);
}

// ---------------- 公共小部件 ----------------
QWidget* MainWindow::makeRow(const QString& label, QWidget* field) {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* l = new QLabel(label);
    l->setFixedWidth(110);
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(l);
    h->addWidget(field, 1);
    return w;
}

QWidget* MainWindow::makePathRow(const QString& label, std::function<QString()> get,
                                 std::function<void(const QString&)> set, const QString& dlgTitle) {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* l = new QLabel(label);
    l->setFixedWidth(110);
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* le = new QLineEdit(get());
    auto* b = new QPushButton("浏览...");
    connect(le, &QLineEdit::textChanged, [set](const QString& t) { set(t); });
    connect(b, &QPushButton::clicked, [this, get, set, dlgTitle, le]() {
        QString cur = get();
        if (cur.isEmpty()) cur = proj.baseDir;
        QString f = QFileDialog::getOpenFileName(this, dlgTitle, cur);
        if (!f.isEmpty()) { set(f); le->setText(f); markDirty(); }
    });
    h->addWidget(l);
    h->addWidget(le, 1);
    h->addWidget(b);
    return w;
}

// ---------------- 设置 ----------------
QWidget* MainWindow::buildSettingsEditor() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    auto* form = new QFormLayout;
    outer->addLayout(form);

    auto* leName = new QLineEdit(proj.projectName);
    connect(leName, &QLineEdit::textChanged, [this](const QString& t) {
        proj.projectName = t; markDirty(); updateCurrentLabel();
    });
    form->addRow("工程名称", leName);

    auto* teStyle = new QPlainTextEdit(proj.settings.styleConstraint);
    teStyle->setPlaceholderText("例如：中式二次元动画风格，新海诚式光影……");
    teStyle->setMinimumHeight(150);
    teStyle->setMaximumHeight(220);
    teStyle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(teStyle, &QPlainTextEdit::textChanged, [this, teStyle]() {
        proj.settings.styleConstraint = teStyle->toPlainText(); markDirty();
    });
    form->addRow("画风约束", teStyle);

    auto* tip = new QLabel("常用画风 / 背景环境声音 / 背景音乐 已在“应用设置”中配置，"
                           "并在“工程提示词生成界面”随时选用。\n"
                           "提示：以上为工程级约束，作用于本工程生成结果。");
    tip->setWordWrap(true);
    tip->setStyleSheet("color:#8B95A7;");
    form->addRow(tip);
    outer->addStretch(1);
    return page;
}

// ---------------- 角色 ----------------
QWidget* MainWindow::buildSubjectsHeader() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->addWidget(new QLabel("角色资源管理"));
    auto* tip = new QLabel("角色包含普通角色与 Q 版内心小人。每个角色配置一张四视图参考图与多个音色音频参考，并可为普通角色关联其 Q 版小人。");
    tip->setWordWrap(true);
    v->addWidget(tip);
    auto* btn = new QPushButton("+ 添加角色");
    connect(btn, &QPushButton::clicked, [this]() {
        Subject s;
        int maxN = 0;
        QRegularExpression re("Subject\\s*(\\d+)");
        for (const auto& x : proj.subjects) {
            auto m = re.match(x.id);
            if (m.hasMatch()) maxN = qMax(maxN, m.captured(1).toInt());
        }
        s.id = QString("Subject %1").arg(maxN + 1);
        proj.subjects.append(s);
        markDirty();
        buildTree();
        selectItem("subject", proj.subjects.size() - 1);
    });
    v->addWidget(btn);
    v->addStretch(1);
    return page;
}

QWidget* MainWindow::buildSubjectEditor(int idx) {
    auto& s = proj.subjects[idx];
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    auto* form = new QFormLayout;
    outer->addLayout(form);

    // 角色编号 (Subject N) 是分镜内的局部编号，此处不配置；id 由程序自动生成。
    auto* leName = new QLineEdit(s.name);
    connect(leName, &QLineEdit::textChanged, [this, idx](const QString& t) {
        proj.subjects[idx].name = t; markDirty(); updateCurrentLabel();
    });
    form->addRow("人物名称", leName);

    auto* leAlias = new QLineEdit(s.alias);
    leAlias->setPlaceholderText("仅用于界面区分，不参与生成（如：本体 / Q版）");
    connect(leAlias, &QLineEdit::textChanged, [this, idx](const QString& t) {
        proj.subjects[idx].alias = t; markDirty(); updateCurrentLabel();
    });
    form->addRow("备注名 (仅界面显示)", leAlias);

    auto* leDesc = new QLineEdit(s.desc);
    connect(leDesc, &QLineEdit::textChanged, [this, idx](const QString& t) {
        proj.subjects[idx].desc = t; markDirty();
    });
    form->addRow("外形 / 描述", leDesc);

    auto* cbQ = new QCheckBox("是否为 Q 版内心小人");
    cbQ->setChecked(s.isQ);
    connect(cbQ, &QCheckBox::toggled, [this, idx](bool b) {
        proj.subjects[idx].isQ = b; markDirty();
    });
    form->addRow(cbQ);

    auto* cbQid = new WheelSafeComboBox;
    cbQid->addItem("(无)", "");
    for (int i = 0; i < proj.subjects.size(); ++i)
        if (i != idx) {
            cbQid->addItem(subjectDisplay(proj.subjects[i]), proj.subjects[i].id);
        }
    cbQid->setCurrentIndex(qMax(0, cbQid->findData(s.qId)));
    connect(cbQid, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, idx, cbQid](int) {
        proj.subjects[idx].qId = cbQid->currentData().toString(); markDirty();
    });
    form->addRow("关联 Q 版小人 (普通角色填)", cbQid);

    form->addRow(makePathRow("四视图参考图",
        [this, idx]() { return proj.subjects[idx].viewImage; },
        [this, idx](const QString& t) { proj.subjects[idx].viewImage = t; markDirty(); },
        "选择四视图参考图（面部特写/全身正侧背 同图）"));

    form->addRow(new QLabel("音色音频参考"));
    auto* auBox = new QWidget;
    auBox->setLayout(new QVBoxLayout);
    auBox->layout()->setContentsMargins(0, 0, 0, 0);
    form->addRow(auBox);
    fillAudios(auBox, idx);
    auto* btnAu = new QPushButton("+ 添加音色音频");
    connect(btnAu, &QPushButton::clicked, [this, idx]() {
        AudioRef a;
        int maxA = 0;
        QRegularExpression re("Audio\\s*(\\d+)");
        for (auto& ss : proj.subjects)
            for (auto& aa : ss.audios) {
                auto m = re.match(aa.id);
                if (m.hasMatch()) maxA = qMax(maxA, m.captured(1).toInt());
            }
        a.id = QString("Audio %1").arg(maxA + 1);
        proj.subjects[idx].audios.append(a);
        markDirty();
        if (auto* c = tree->currentItem()) buildEditor(c);
    });
    auto* btnRow = new QWidget;
    auto* bh = new QHBoxLayout(btnRow);
    bh->setContentsMargins(0, 8, 0, 0);
    auto* btnDel = new QPushButton("删除该角色");
    connect(btnDel, &QPushButton::clicked, [this, idx]() {
        QString removedId = proj.subjects[idx].id;
        proj.subjects.removeAt(idx);
        for (auto& seq : proj.sequences) seq.cast.removeAll(removedId);
        markDirty();
        buildTree();
        selectItem("subjects_header", -1);
    });
    bh->addWidget(btnAu);
    bh->addStretch(1);
    bh->addWidget(btnDel);
    outer->addWidget(btnRow);
    outer->addStretch(1);
    return page;
}

void MainWindow::fillAudios(QWidget* container, int sIdx) {
    if (!container->layout()) container->setLayout(new QVBoxLayout);
    clearLayout(container->layout());
    auto* lay = container->layout();
    for (int ai = 0; ai < proj.subjects[sIdx].audios.size(); ++ai) {
        auto& a = proj.subjects[sIdx].audios[ai];
        auto* g = new QGroupBox(a.name.trimmed().isEmpty() ? QString("音频 #%1").arg(ai + 1) : a.name);
        auto* f = new QFormLayout(g);

        // 音频名称（界面显示用；内部编号 id 已内置，不再提供编辑）
        auto* leName = new QLineEdit(a.name);
        leName->setPlaceholderText("如：温柔女声；留空时选择文件会自动用文件名填充");
        connect(leName, &QLineEdit::textChanged, [this, sIdx, ai, g](const QString& t) {
            proj.subjects[sIdx].audios[ai].name = t;
            g->setTitle(t.trimmed().isEmpty()
                            ? QString("音频 #%1").arg(ai + 1)
                            : t);
            markDirty();
        });
        f->addRow("音频名称", leName);
        f->addRow(makePathRow("音频文件",
            [this, sIdx, ai]() { return proj.subjects[sIdx].audios[ai].path; },
            [this, sIdx, ai, leName](const QString& t) {
                auto& au = proj.subjects[sIdx].audios[ai];
                au.path = t;
                // 名称为空时，用所选音频的文件名作为名称（仅对已存在的文件生效，避免手输路径时误填）
                if (au.name.trimmed().isEmpty() && !t.isEmpty() && QFileInfo(t).isFile()) {
                    QString base = QFileInfo(t).completeBaseName();
                    if (!base.isEmpty()) au.name = base;
                }
                if (leName && leName->text() != au.name) leName->setText(au.name);
                markDirty();
            },
            "选择音频文件"));
        auto* teDesc = new QPlainTextEdit(a.desc);
        teDesc->setMinimumHeight(56);
        teDesc->setMaximumHeight(90);
        teDesc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(teDesc, &QPlainTextEdit::textChanged, [this, sIdx, ai, teDesc]() {
            proj.subjects[sIdx].audios[ai].desc = teDesc->toPlainText(); markDirty();
        });
        f->addRow("说明", teDesc);
        auto* btnDel = new QPushButton("删除此音频");
        connect(btnDel, &QPushButton::clicked, [this, sIdx, ai]() {
            proj.subjects[sIdx].audios.removeAt(ai);
            if (auto* c = tree->currentItem()) buildEditor(c);
        });
        f->addRow(btnDel);
        lay->addWidget(g);
    }
}

// ---------------- 场景 ----------------
QWidget* MainWindow::buildScenesHeader() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->addWidget(new QLabel("场景图片资源管理"));
    auto* btn = new QPushButton("+ 添加场景");
    connect(btn, &QPushButton::clicked, [this]() {
        Scene s;
        int maxN = 0;
        QRegularExpression re("Scene\\s*(\\d+)");
        for (const auto& x : proj.scenes) {
            auto m = re.match(x.id);
            if (m.hasMatch()) maxN = qMax(maxN, m.captured(1).toInt());
        }
        s.id = QString("Scene %1").arg(maxN + 1);
        proj.scenes.append(s);
        markDirty();
        buildTree();
        selectItem("scene", proj.scenes.size() - 1);
    });
    v->addWidget(btn);
    v->addStretch(1);
    return page;
}

QWidget* MainWindow::buildSceneEditor(int idx) {
    auto& s = proj.scenes[idx];
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    auto* form = new QFormLayout;
    outer->addLayout(form);
    // 场景编号 (Scene N) 为分镜内局部编号，此处不配置；id 由程序自动生成。
    auto* leLbl = new QLineEdit(s.label);
    connect(leLbl, &QLineEdit::textChanged, [this, idx](const QString& t) {
        proj.scenes[idx].label = t; markDirty(); updateCurrentLabel();
    });
    form->addRow("名称", leLbl);
    form->addRow(makePathRow("场景图片",
        [this, idx]() { return proj.scenes[idx].path; },
        [this, idx](const QString& t) { proj.scenes[idx].path = t; markDirty(); },
        "选择场景图片"));
    auto* teDesc = new QPlainTextEdit(s.desc);
    teDesc->setPlaceholderText("描述该场景的画面内容、光线、氛围等，将作为场景定义的说明输出。");
    teDesc->setMinimumHeight(140);
    teDesc->setMaximumHeight(240);
    teDesc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(teDesc, &QPlainTextEdit::textChanged, [this, idx, teDesc]() {
        proj.scenes[idx].desc = teDesc->toPlainText(); markDirty();
    });
    form->addRow("场景说明 (生成时以 “<Subject N> 是 <Picture N>，” 前缀输出)", teDesc);
    auto* btnDel = new QPushButton("删除该场景");
    connect(btnDel, &QPushButton::clicked, [this, idx]() {
        const QString removedId = proj.scenes[idx].id;
        proj.scenes.removeAt(idx);
        for (auto& seq : proj.sequences) seq.scenes.removeAll(removedId);
        markDirty();
        buildTree();
        selectItem("scenes_header", -1);
    });
    auto* btnRow = new QWidget;
    auto* bh = new QHBoxLayout(btnRow);
    bh->setContentsMargins(0, 8, 0, 0);
    bh->addStretch(1);
    bh->addWidget(btnDel);
    outer->addWidget(btnRow);
    outer->addStretch(1);
    return page;
}

// ---------------- 道具 ----------------
QWidget* MainWindow::buildPropsHeader() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->addWidget(new QLabel("道具图片资源管理"));
    auto* btn = new QPushButton("+ 添加道具");
    connect(btn, &QPushButton::clicked, [this]() {
        Prop p;
        int maxN = 0;
        QRegularExpression re("Prop\\s*(\\d+)");
        for (const auto& x : proj.props) {
            auto m = re.match(x.id);
            if (m.hasMatch()) maxN = qMax(maxN, m.captured(1).toInt());
        }
        p.id = QString("Prop %1").arg(maxN + 1);
        proj.props.append(p);
        markDirty();
        buildTree();
        selectItem("prop", proj.props.size() - 1);
    });
    v->addWidget(btn);
    v->addStretch(1);
    return page;
}

QWidget* MainWindow::buildPropEditor(int idx) {
    auto& s = proj.props[idx];
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    auto* form = new QFormLayout;
    outer->addLayout(form);
    // 道具编号 (Prop N) 为分镜内局部编号，此处不配置；id 由程序自动生成。
    auto* leLbl = new QLineEdit(s.label);
    connect(leLbl, &QLineEdit::textChanged, [this, idx](const QString& t) {
        proj.props[idx].label = t; markDirty(); updateCurrentLabel();
    });
    form->addRow("名称", leLbl);
    form->addRow(makePathRow("道具图片",
        [this, idx]() { return proj.props[idx].path; },
        [this, idx](const QString& t) { proj.props[idx].path = t; markDirty(); },
        "选择道具图片"));
    auto* teDesc = new QPlainTextEdit(s.desc);
    teDesc->setPlaceholderText("描述该道具的外观、材质、颜色等，将作为道具定义的说明输出。");
    teDesc->setMinimumHeight(140);
    teDesc->setMaximumHeight(240);
    teDesc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(teDesc, &QPlainTextEdit::textChanged, [this, idx, teDesc]() {
        proj.props[idx].desc = teDesc->toPlainText(); markDirty();
    });
    form->addRow("道具说明 (生成时以 “<Subject N> 是 <Picture N>，” 前缀输出)", teDesc);
    auto* btnDel = new QPushButton("删除该道具");
    connect(btnDel, &QPushButton::clicked, [this, idx]() {
        const QString removedId = proj.props[idx].id;
        proj.props.removeAt(idx);
        for (auto& seq : proj.sequences) seq.props.removeAll(removedId);
        markDirty();
        buildTree();
        selectItem("props_header", -1);
    });
    auto* btnRow = new QWidget;
    auto* bh = new QHBoxLayout(btnRow);
    bh->setContentsMargins(0, 8, 0, 0);
    bh->addStretch(1);
    bh->addWidget(btnDel);
    outer->addWidget(btnRow);
    outer->addStretch(1);
    return page;
}

// ---------------- 分镜（序列容器） ----------------
QWidget* MainWindow::buildSequencesHeader() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->addWidget(new QLabel("分镜管理（视频级容器，每个分镜可含多个镜头）"));
    auto* tip = new QLabel("分镜按顺序自动编号（无需手动编号，无名称时显示“分镜N”）；"
                          "分镜下可添加多个镜头(Shot)，每个镜头含段落(SubShot)与台词。"
                          "人物 / 场景 / 道具在本分镜内统一编号为 <Subject N>（顺序：人物 → 场景 → 道具，与项目编号无关）；"
                          "caption 里仍用 <Subject N>/<Scene N>/<Prop N> 引用项目资源，生成时会自动转换为统一的 <Subject N>。");
    tip->setWordWrap(true);
    v->addWidget(tip);
    auto* btn = new QPushButton("+ 添加分镜");
    connect(btn, &QPushButton::clicked, [this]() {
        Sequence seq;
        int maxN = 0;
        QRegularExpression re("^Seq\\s+(\\d+)$");
        for (const auto& x : proj.sequences) {
            auto m = re.match(x.id);
            if (m.hasMatch()) maxN = qMax(maxN, m.captured(1).toInt());
        }
        seq.id = QString("Seq %1").arg(maxN + 1);
        seq.shots.append(Shot());
        proj.sequences.append(seq);
        markDirty();
        buildTree();
        selectItem("sequence", proj.sequences.size() - 1);
    });
    v->addWidget(btn);
    v->addStretch(1);
    return page;
}

QWidget* MainWindow::buildSequenceEditor(int seqIdx) {
    auto& seq = proj.sequences[seqIdx];
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);

    // 顶部快捷操作：预览/编辑提示词、插入/复制/删除分镜（避免滚到底部才能点）
    auto* topActions = new QWidget;
    auto* tah = new QHBoxLayout(topActions);
    tah->setContentsMargins(0, 0, 0, 0);
    auto* btnPreview = new QPushButton("预览 / 编辑当前分镜提示词");
    auto* btnInsSeq = new QPushButton("插入分镜");
    auto* btnDupSeq = new QPushButton("复制该分镜");
    auto* btnDelSeq = new QPushButton("删除该分镜");
    auto* btnUp = new QPushButton("↑上移");
    auto* btnDown = new QPushButton("↓下移");
    tah->addWidget(btnPreview);
    tah->addWidget(btnInsSeq);
    tah->addWidget(btnDupSeq);
    tah->addWidget(btnDelSeq);
    tah->addWidget(btnUp);
    tah->addWidget(btnDown);
    tah->addStretch(1);
    v->addWidget(topActions);

    // 生成不冲突的下一个分镜内部编号（Seq N）
    auto nextSeqId = [this]() -> QString {
        int maxN = 0;
        QRegularExpression re("^Seq\\s+(\\d+)$");
        for (const auto& x : proj.sequences) {
            auto m = re.match(x.id);
            if (m.hasMatch()) maxN = qMax(maxN, m.captured(1).toInt());
        }
        return QString("Seq %1").arg(maxN + 1);
    };

    connect(btnInsSeq, &QPushButton::clicked, [this, seqIdx, nextSeqId]() {
        Sequence seq;
        seq.id = nextSeqId();
        seq.shots.append(Shot());
        proj.sequences.insert(seqIdx + 1, seq);
        markDirty(); buildTree(); selectItem("sequence", seqIdx + 1);
    });
    connect(btnDupSeq, &QPushButton::clicked, [this, seqIdx, nextSeqId]() {
        Sequence copy = proj.sequences[seqIdx];   // 深拷贝（含镜头/段落/台词）
        copy.id = nextSeqId();
        copy.shotFiles.clear();   // 保存时按全局顺序重新分配文件名，避免复用旧文件名
        if (!copy.name.trimmed().isEmpty()) copy.name += " 副本";
        proj.sequences.insert(seqIdx + 1, copy);
        markDirty(); buildTree(); selectItem("sequence", seqIdx + 1);
    });
    connect(btnDelSeq, &QPushButton::clicked, [this, seqIdx]() {
        proj.sequences.removeAt(seqIdx);
        markDirty(); buildTree(); selectItem("sequences_header", -1);
    });

    // 分镜名称（可选；无则按列表顺序显示“分镜N”）
    auto* nameRow = new QWidget;
    auto* nh = new QHBoxLayout(nameRow);
    nh->setContentsMargins(0, 0, 0, 0);
    auto* leName = new QLineEdit(seq.name);
    leName->setPlaceholderText("分镜名称（可选，用于显示；不填则显示“分镜N”）");
    connect(leName, &QLineEdit::textChanged, [this, seqIdx](const QString& t) {
        proj.sequences[seqIdx].name = t; markDirty(); updateCurrentLabel();
    });
    connect(btnUp, &QPushButton::clicked, [this, seqIdx]() {
        if (seqIdx <= 0) return;
        proj.sequences.swapItemsAt(seqIdx - 1, seqIdx);
        markDirty(); buildTree(); selectItem("sequence", seqIdx - 1);
    });
    connect(btnDown, &QPushButton::clicked, [this, seqIdx]() {
        if (seqIdx >= proj.sequences.size() - 1) return;
        proj.sequences.swapItemsAt(seqIdx, seqIdx + 1);
        markDirty(); buildTree(); selectItem("sequence", seqIdx + 1);
    });
    nh->addWidget(new QLabel("分镜名称"));
    nh->addWidget(leName, 1);
    v->addWidget(nameRow);

    auto* teLip = new QPlainTextEdit(seq.lipConstraint);
    teLip->setPlaceholderText("本分镜的嘴型约束（可留空）");
    teLip->setMinimumHeight(80);
    teLip->setMaximumHeight(120);
    teLip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(teLip, &QPlainTextEdit::textChanged, [this, seqIdx, teLip]() {
        proj.sequences[seqIdx].lipConstraint = teLip->toPlainText(); markDirty();
    });
    v->addWidget(new QLabel("嘴型约束 (本分镜)"));
    v->addWidget(teLip);

    // 出场角色 / 场景 / 道具：本分镜统一编号为 <Subject N>（人物 → 场景 → 道具）
    auto* castGroup = new QGroupBox("出场角色（人物，顺序即 <Subject N> 编号，从项目角色中选择）");
    auto* castLay = new QVBoxLayout(castGroup);
    auto* castBox = new QWidget;
    castBox->setLayout(new QVBoxLayout);
    castBox->layout()->setContentsMargins(0, 0, 0, 0);
    castLay->addWidget(castBox);
    fillCast(castBox, seqIdx);
    v->addWidget(castGroup);

    auto* sceneGroup = new QGroupBox("场景（本分镜使用，生成时并入 <Subject N>，从项目场景中选择）");
    auto* sceneLay = new QVBoxLayout(sceneGroup);
    auto* sceneBox = new QWidget;
    sceneBox->setLayout(new QVBoxLayout);
    sceneBox->layout()->setContentsMargins(0, 0, 0, 0);
    sceneLay->addWidget(sceneBox);
    fillScenes(sceneBox, seqIdx);
    v->addWidget(sceneGroup);

    auto* propGroup = new QGroupBox("道具（本分镜使用，生成时并入 <Subject N>，从项目道具中选择）");
    auto* propLay = new QVBoxLayout(propGroup);
    auto* propBox = new QWidget;
    propBox->setLayout(new QVBoxLayout);
    propBox->layout()->setContentsMargins(0, 0, 0, 0);
    propLay->addWidget(propBox);
    fillProps(propBox, seqIdx);
    v->addWidget(propGroup);

    // 镜头列表（可上下移动 / 编辑 / 删除）
    v->addWidget(new QLabel("镜头 (Shot)："));
    auto* shotBox = new QWidget;
    shotBox->setLayout(new QVBoxLayout);
    shotBox->layout()->setContentsMargins(0, 0, 0, 0);
    v->addWidget(shotBox);
    {
        auto* lay = shotBox->layout();
        for (int j = 0; j < seq.shots.size(); ++j) {
            auto* row = new QWidget;
            auto* hl = new QHBoxLayout(row);
            QString sn = seq.shots[j].label.trimmed().isEmpty()
                             ? QString("镜头%1").arg(j + 1) : seq.shots[j].label;
            hl->addWidget(new QLabel(QString("%1. %2").arg(j + 1).arg(sn)));
            auto* up = new QPushButton("↑");
            auto* down = new QPushButton("↓");
            auto* edit = new QPushButton("编辑");
            auto* del = new QPushButton("删除");
            connect(up, &QPushButton::clicked, [this, seqIdx, j]() {
                if (j <= 0) return;
                proj.sequences[seqIdx].shots.swapItemsAt(j - 1, j);
                markDirty(); buildEditor(tree->currentItem());
            });
            connect(down, &QPushButton::clicked, [this, seqIdx, j]() {
                if (j >= proj.sequences[seqIdx].shots.size() - 1) return;
                proj.sequences[seqIdx].shots.swapItemsAt(j, j + 1);
                markDirty(); buildEditor(tree->currentItem());
            });
            connect(edit, &QPushButton::clicked, [this, seqIdx, j]() { selectShotNode(seqIdx, j); });
            connect(del, &QPushButton::clicked, [this, seqIdx, j]() {
                proj.sequences[seqIdx].shots.removeAt(j);
                if (proj.sequences[seqIdx].shots.isEmpty())
                    proj.sequences[seqIdx].shots.append(Shot());
                markDirty(); buildEditor(tree->currentItem());
            });
            hl->addWidget(up); hl->addWidget(down); hl->addWidget(edit); hl->addWidget(del);
            lay->addWidget(row);
        }
        auto* addRow = new QWidget;
        auto* ahl = new QHBoxLayout(addRow);
        auto* addBtn = new QPushButton("+ 添加镜头");
        connect(addBtn, &QPushButton::clicked, [this, seqIdx]() {
            proj.sequences[seqIdx].shots.append(Shot());
            markDirty(); buildEditor(tree->currentItem());
        });
        ahl->addWidget(addBtn);
        lay->addWidget(addRow);
    }

    v->addStretch(1);

    // 第 2 页：嵌入提示词预览 / 编辑面板（可对照结构化源数据修改，点“更新配置”保存）
    auto* previewPage = new QWidget;
    auto* pv = new QVBoxLayout(previewPage);
    pv->setContentsMargins(0, 0, 0, 0);
    auto* bar = new QWidget;
    auto* bh = new QHBoxLayout(bar);
    bh->setContentsMargins(8, 6, 8, 6);
    QString seqNm = seq.name.trimmed();
    auto* back = new QPushButton(QString("← 返回分镜 %1%2")
        .arg(seqIdx + 1).arg(seqNm.isEmpty() ? "" : ("：" + seqNm)));
    bh->addWidget(back);
    auto* title = new QLabel("提示词预览 / 编辑：可在此直接修改，并对照右侧结构化源数据；改完点“更新配置”保存。");
    title->setWordWrap(true);
    bh->addWidget(title, 1);
    pv->addWidget(bar);

    auto* pd = new PreviewDock(&proj, &appCfg, seqIdx);
    connect(pd, &PreviewDock::projectEdited, this, [this]() {
        markDirty();
        statusBar()->showMessage("分镜提示词覆盖已更新，记得保存工程", 6000);
    });
    connect(pd, &PreviewDock::requestOpenShot, this, [this, seqIdx](int shotIdx) {
        selectShotNode(seqIdx, shotIdx);   // 跳转到对应镜头的编辑页
    });
    pv->addWidget(pd, 1);

    auto* stack = new QStackedWidget;
    stack->addWidget(page);        // 第 1 页：分镜编辑
    stack->addWidget(previewPage); // 第 2 页：提示词预览 / 编辑
    connect(btnPreview, &QPushButton::clicked, stack, [this, stack, pd]() {
        seqPreviewMode = true;
        pd->refresh();
        stack->setCurrentIndex(1);
    });
    connect(back, &QPushButton::clicked, stack, [this, stack]() {
        seqPreviewMode = false;
        stack->setCurrentIndex(0);
    });
    // 沿用上次停留的页面：之前在预览页则直接进预览页，否则进分镜编辑页
    stack->setCurrentIndex(seqPreviewMode ? 1 : 0);
    return stack;
}

QWidget* MainWindow::buildShotEditor(int seqIdx, int shotIdx) {
    auto& sh = proj.sequences[seqIdx].shots[shotIdx];
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    auto* form = new QFormLayout;
    outer->addLayout(form);
    auto* leLbl = new QLineEdit(sh.label);
    connect(leLbl, &QLineEdit::textChanged, [this, seqIdx, shotIdx](const QString& t) {
        proj.sequences[seqIdx].shots[shotIdx].label = t; markDirty(); updateCurrentLabel();
    });
    form->addRow("镜头名称 (可选)", leLbl);

    form->addRow(new QLabel("段落 (SubShot) 与台词"));
    auto* ssBox = new QWidget;
    ssBox->setLayout(new QVBoxLayout);
    ssBox->layout()->setContentsMargins(0, 0, 0, 0);
    form->addRow(ssBox);
    fillSubShots(ssBox, seqIdx, shotIdx);

    auto* btnRow = new QWidget;
    auto* bh = new QHBoxLayout(btnRow);
    bh->setContentsMargins(0, 8, 0, 0);
    auto* btnAdd = new QPushButton("+ 添加段落");
    connect(btnAdd, &QPushButton::clicked, [this, seqIdx, shotIdx]() {
        proj.sequences[seqIdx].shots[shotIdx].subShots.append(SubShot());
        if (auto* c = tree->currentItem()) buildEditor(c);
    });
    auto* btnBack = new QPushButton("返回分镜");
    connect(btnBack, &QPushButton::clicked, [this, seqIdx]() { selectItem("sequence", seqIdx); });
    bh->addWidget(btnAdd);
    bh->addStretch(1);
    bh->addWidget(btnBack);
    outer->addWidget(btnRow);
    outer->addStretch(1);
    return page;
}

void MainWindow::fillCast(QWidget* container, int seqIdx) {
    if (!container->layout()) container->setLayout(new QVBoxLayout);
    clearLayout(container->layout());
    auto* lay = container->layout();
    auto& cast = proj.sequences[seqIdx].cast;

    auto nameOf = [this](const QString& id) -> QString {
        for (const auto& s : proj.subjects) if (s.id == id) return subjectDisplay(s);
        return "未命名角色";
    };

    for (int i = 0; i < cast.size(); ++i) {
        auto* row = new QWidget;
        auto* hl = new QHBoxLayout(row);
        hl->addWidget(new QLabel(QString("%1. %2").arg(i + 1).arg(nameOf(cast[i]))));
        auto* del = new QPushButton("移除");
        connect(del, &QPushButton::clicked, [this, seqIdx, i]() {
            proj.sequences[seqIdx].cast.removeAt(i); markDirty();
            if (auto* c = tree->currentItem()) buildEditor(c);
        });
        hl->addWidget(del);
        lay->addWidget(row);
    }

    auto* addRow = new QWidget;
    auto* hl2 = new QHBoxLayout(addRow);
    auto* combo = new WheelSafeComboBox;
    for (const auto& s : proj.subjects)
        if (!cast.contains(s.id)) combo->addItem(subjectDisplay(s), s.id);
    combo->setPlaceholderText("选择要添加的角色…");
    auto* addBtn = new QPushButton("+ 添加出场角色");
    connect(addBtn, &QPushButton::clicked, [this, seqIdx, combo]() {
        QString id = combo->currentData().toString();
        if (!id.isEmpty() && !proj.sequences[seqIdx].cast.contains(id)) {
            proj.sequences[seqIdx].cast.append(id); markDirty();
            if (auto* c = tree->currentItem()) buildEditor(c);
        }
    });
    hl2->addWidget(combo);
    hl2->addWidget(addBtn);
    lay->addWidget(addRow);

    if (proj.subjects.isEmpty())
        lay->addWidget(new QLabel("项目中暂无任何角色，请先在“角色”下添加。"));
}

void MainWindow::fillScenes(QWidget* container, int seqIdx) {
    if (!container->layout()) container->setLayout(new QVBoxLayout);
    clearLayout(container->layout());
    auto* lay = container->layout();
    auto& list = proj.sequences[seqIdx].scenes;

    auto nameOf = [this](const QString& id) -> QString {
        for (int k = 0; k < proj.scenes.size(); ++k)
            if (proj.scenes[k].id == id)
                return proj.scenes[k].label.trimmed().isEmpty()
                           ? QString("场景%1").arg(k + 1) : proj.scenes[k].label;
        return "场景";
    };

    for (int i = 0; i < list.size(); ++i) {
        auto* row = new QWidget;
        auto* hl = new QHBoxLayout(row);
        hl->addWidget(new QLabel(QString("%1. %2").arg(i + 1).arg(nameOf(list[i]))));
        auto* del = new QPushButton("移除");
        connect(del, &QPushButton::clicked, [this, seqIdx, i]() {
            proj.sequences[seqIdx].scenes.removeAt(i); markDirty();
            if (auto* c = tree->currentItem()) buildEditor(c);
        });
        hl->addWidget(del);
        lay->addWidget(row);
    }

    auto* addRow = new QWidget;
    auto* hl2 = new QHBoxLayout(addRow);
    auto* combo = new WheelSafeComboBox;
    for (int k = 0; k < proj.scenes.size(); ++k) {
        const auto& s = proj.scenes[k];
        if (!list.contains(s.id))
            combo->addItem(s.label.trimmed().isEmpty() ? QString("场景%1").arg(k + 1) : s.label, s.id);
    }
    combo->setPlaceholderText("从已添加的场景中选择…");
    auto* addBtn = new QPushButton("+ 添加场景");
    connect(addBtn, &QPushButton::clicked, [this, seqIdx, combo]() {
        QString id = combo->currentData().toString();
        if (!id.isEmpty() && !proj.sequences[seqIdx].scenes.contains(id)) {
            proj.sequences[seqIdx].scenes.append(id); markDirty();
            if (auto* c = tree->currentItem()) buildEditor(c);
        }
    });
    hl2->addWidget(combo);
    hl2->addWidget(addBtn);
    lay->addWidget(addRow);

    if (proj.scenes.isEmpty())
        lay->addWidget(new QLabel("项目中暂无任何场景，请先在“场景”下添加。"));
}

void MainWindow::fillProps(QWidget* container, int seqIdx) {
    if (!container->layout()) container->setLayout(new QVBoxLayout);
    clearLayout(container->layout());
    auto* lay = container->layout();
    auto& list = proj.sequences[seqIdx].props;

    auto nameOf = [this](const QString& id) -> QString {
        for (int k = 0; k < proj.props.size(); ++k)
            if (proj.props[k].id == id)
                return proj.props[k].label.trimmed().isEmpty()
                           ? QString("道具%1").arg(k + 1) : proj.props[k].label;
        return "道具";
    };

    for (int i = 0; i < list.size(); ++i) {
        auto* row = new QWidget;
        auto* hl = new QHBoxLayout(row);
        hl->addWidget(new QLabel(QString("%1. %2").arg(i + 1).arg(nameOf(list[i]))));
        auto* del = new QPushButton("移除");
        connect(del, &QPushButton::clicked, [this, seqIdx, i]() {
            proj.sequences[seqIdx].props.removeAt(i); markDirty();
            if (auto* c = tree->currentItem()) buildEditor(c);
        });
        hl->addWidget(del);
        lay->addWidget(row);
    }

    auto* addRow = new QWidget;
    auto* hl2 = new QHBoxLayout(addRow);
    auto* combo = new WheelSafeComboBox;
    for (int k = 0; k < proj.props.size(); ++k) {
        const auto& s = proj.props[k];
        if (!list.contains(s.id))
            combo->addItem(s.label.trimmed().isEmpty() ? QString("道具%1").arg(k + 1) : s.label, s.id);
    }
    combo->setPlaceholderText("从已添加的道具中选择…");
    auto* addBtn = new QPushButton("+ 添加道具");
    connect(addBtn, &QPushButton::clicked, [this, seqIdx, combo]() {
        QString id = combo->currentData().toString();
        if (!id.isEmpty() && !proj.sequences[seqIdx].props.contains(id)) {
            proj.sequences[seqIdx].props.append(id); markDirty();
            if (auto* c = tree->currentItem()) buildEditor(c);
        }
    });
    hl2->addWidget(combo);
    hl2->addWidget(addBtn);
    lay->addWidget(addRow);

    if (proj.props.isEmpty())
        lay->addWidget(new QLabel("项目中暂无任何道具，请先在“道具”下添加。"));
}

void MainWindow::fillSubShots(QWidget* container, int seqIdx, int shotIdx) {
    if (!container->layout()) container->setLayout(new QVBoxLayout);
    clearLayout(container->layout());
    auto* lay = container->layout();
    for (int si = 0; si < proj.sequences[seqIdx].shots[shotIdx].subShots.size(); ++si) {
        auto* g = new QGroupBox(QString("段落 #%1").arg(si + 1));
        auto* v = new QVBoxLayout(g);
        auto* teCap = new QPlainTextEdit(proj.sequences[seqIdx].shots[shotIdx].subShots[si].caption);
        teCap->setPlaceholderText("段落画面描述（可含 <Subject N>/<Scene N>/<Prop N> 引用）");
        teCap->setMinimumHeight(90);
        teCap->setMaximumHeight(180);
        teCap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(teCap, &QPlainTextEdit::textChanged, [this, seqIdx, shotIdx, si, teCap]() {
            proj.sequences[seqIdx].shots[shotIdx].subShots[si].caption = teCap->toPlainText(); markDirty();
        });
        v->addWidget(teCap);

        v->addWidget(new QLabel("台词："));
        auto* spBox = new QWidget;
        spBox->setLayout(new QVBoxLayout);
        spBox->layout()->setContentsMargins(0, 0, 0, 0);
        v->addWidget(spBox);
        fillSpeeches(spBox, seqIdx, shotIdx, si);

        auto* btnAddSp = new QPushButton("+ 添加台词");
        connect(btnAddSp, &QPushButton::clicked, [this, seqIdx, shotIdx, si]() {
            proj.sequences[seqIdx].shots[shotIdx].subShots[si].speeches.append(Speech());
            if (auto* c = tree->currentItem()) buildEditor(c);
        });
        v->addWidget(btnAddSp);

        auto* btnDel = new QPushButton("删除段落");
        connect(btnDel, &QPushButton::clicked, [this, seqIdx, shotIdx, si]() {
            proj.sequences[seqIdx].shots[shotIdx].subShots.removeAt(si);
            if (auto* c = tree->currentItem()) buildEditor(c);
        });
        v->addWidget(btnDel);
        lay->addWidget(g);
    }
}

void MainWindow::fillSpeeches(QWidget* container, int seqIdx, int shotIdx, int subIdx) {
    if (!container->layout()) container->setLayout(new QVBoxLayout);
    clearLayout(container->layout());
    auto* lay = container->layout();

    auto findSubj = [this](const QString& id) -> const Subject* {
        for (const auto& s : proj.subjects) if (s.id == id) return &s;
        return nullptr;
    };
    // 角色显示名：名称（备注名）；无名称则用编号
    auto subjLabel = [](const Subject& s) -> QString {
        QString n = s.name.trimmed().isEmpty() ? s.id : s.name.trimmed();
        if (!s.alias.trimmed().isEmpty()) n += "（" + s.alias.trimmed() + "）";
        return n;
    };
    // 音色显示名：音频名称（空则用文件名；内部编号不再对外显示）
    auto audioLabel = [](const AudioRef& a) -> QString {
        QString n = a.name.trimmed();
        if (n.isEmpty() && !a.path.isEmpty()) n = QFileInfo(a.path).completeBaseName();
        return n.isEmpty() ? QString("（未命名音频）") : n;
    };

    // 候选说话角色只取“本分镜已加入的角色”（cast）；分镜未配置出场角色时，
    // 镜头里的台词不再提供任何可选的说话角色，也不提供其音色
    QStringList allowed;
    for (const auto& id : proj.sequences[seqIdx].cast)
        if (findSubj(id)) allowed << id;
    const bool hasCast = !allowed.isEmpty();

    auto& sps = proj.sequences[seqIdx].shots[shotIdx].subShots[subIdx].speeches;
    for (int k = 0; k < sps.size(); ++k) {
        auto* g = new QGroupBox(QString("台词 #%1").arg(k + 1));
        auto* f = new QFormLayout(g);

        auto* cbSub = new WheelSafeComboBox;
        if (!hasCast) {
            cbSub->addItem("（本分镜未配置出场角色）", QString());
            cbSub->setEnabled(false);
        } else {
            QStringList ids = allowed;
            // 历史数据里可能存有不在 cast 内的角色，保留以免被静默改掉
            if (!sps[k].subject.isEmpty() && !ids.contains(sps[k].subject))
                ids << sps[k].subject;
            for (const auto& id : ids) {
                const Subject* s = findSubj(id);
                cbSub->addItem(s ? subjLabel(*s) : id, id);
                cbSub->setItemData(cbSub->count() - 1, id, Qt::ToolTipRole);
            }
            int si = cbSub->findData(sps[k].subject);
            if (si < 0) si = 0;
            cbSub->setCurrentIndex(si);
            // 数据里没存说话角色时，把下拉默认选中的角色写回，避免“界面显示已选、数据却为空”
            sps[k].subject = cbSub->itemData(si).toString();
        }
        f->addRow("说话角色", cbSub);

        auto* cbAudio = new WheelSafeComboBox;
        // 只列出该角色“已配置”（有名称或有文件）的音色；未配置的空音频槽和其他角色的
        // 历史音色都不再出现，避免下拉里残留不正确的选项。
        auto populateAudio = [cbAudio, findSubj, audioLabel](const QString& subjId,
                                                            const QString& want,
                                                            bool preferFirst) -> QString {
            QSignalBlocker b(cbAudio);
            cbAudio->clear();
            cbAudio->addItem("（无音色音频）", QString());
            const Subject* s = findSubj(subjId);
            if (s)
                for (const auto& a : s->audios) {
                    // 未命名且无文件的音频不构成可用选项
                    if (a.name.trimmed().isEmpty() && a.path.trimmed().isEmpty()) continue;
                    cbAudio->addItem(audioLabel(a), a.id);
                    cbAudio->setItemData(cbAudio->count() - 1, a.desc, Qt::ToolTipRole);
                }
            int ai = want.isEmpty() ? -1 : cbAudio->findData(want);
            if (ai < 0) {
                // 有效匹配优先；未指定时（切角色）取第一个可用音色；否则空音频
                ai = (preferFirst && cbAudio->count() > 1) ? 1 : 0;
            }
            cbAudio->setCurrentIndex(ai);
            return cbAudio->itemData(ai).toString();
        };
        if (!hasCast) {
            cbAudio->addItem("（无出场角色，无音色可选）", QString());
            cbAudio->setEnabled(false);
        } else {
            // 载入时把指向“未配置/不存在”音色的旧值纠正为空音频，保证下拉与数据一致
            QString resolved = populateAudio(sps[k].subject, sps[k].audio, /*preferFirst=*/false);
            if (!sps[k].audio.isEmpty() && resolved != sps[k].audio) sps[k].audio = resolved;
        }
        f->addRow("音色音频", cbAudio);

        connect(cbSub, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, seqIdx, shotIdx, subIdx, k, cbSub, populateAudio](int) {
            auto& arr = proj.sequences[seqIdx].shots[shotIdx].subShots[subIdx].speeches;
            if (k >= arr.size()) return;
            arr[k].subject = cbSub->currentData().toString();
            // 切换角色后，音色自动切到该角色可用的首个音频，避免配错
            arr[k].audio = populateAudio(arr[k].subject, QString(), /*preferFirst=*/true);
            markDirty();
        });
        connect(cbAudio, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, seqIdx, shotIdx, subIdx, k, cbAudio](int) {
            auto& arr = proj.sequences[seqIdx].shots[shotIdx].subShots[subIdx].speeches;
            if (k >= arr.size()) return;
            arr[k].audio = cbAudio->currentData().toString();
            markDirty();
        });

        auto* leNote = new QLineEdit(sps[k].note);
        connect(leNote, &QLineEdit::textChanged, [this, seqIdx, shotIdx, subIdx, k](const QString& t) {
            proj.sequences[seqIdx].shots[shotIdx].subShots[subIdx].speeches[k].note = t; markDirty();
        });
        f->addRow("备注 (说话话式, 空则无)", leNote);

        auto* leLead = new QLineEdit(sps[k].lead);
        leLead->setPlaceholderText("如：担心的");
        connect(leLead, &QLineEdit::textChanged, [this, seqIdx, shotIdx, subIdx, k](const QString& t) {
            proj.sequences[seqIdx].shots[shotIdx].subShots[subIdx].speeches[k].lead = t; markDirty();
        });
        f->addRow("前导修饰", leLead);

        auto* teText = new QPlainTextEdit(sps[k].text);
        teText->setPlaceholderText("说话内容（中文引号自动添加）");
        teText->setMinimumHeight(56);
        teText->setMaximumHeight(110);
        teText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(teText, &QPlainTextEdit::textChanged, [this, seqIdx, shotIdx, subIdx, k, teText]() {
            proj.sequences[seqIdx].shots[shotIdx].subShots[subIdx].speeches[k].text = teText->toPlainText(); markDirty();
        });
        f->addRow("说话内容", teText);

        auto* btnDel = new QPushButton("删除台词");
        connect(btnDel, &QPushButton::clicked, [this, seqIdx, shotIdx, subIdx, k]() {
            proj.sequences[seqIdx].shots[shotIdx].subShots[subIdx].speeches.removeAt(k);
            if (auto* c = tree->currentItem()) buildEditor(c);
        });
        f->addRow(btnDel);
        lay->addWidget(g);
    }
}

// ---------------- 菜单动作 ----------------
void MainWindow::markDirty() {
    dirty = true;
    setWindowTitle(QString("%1 - H3Hint *").arg(proj.projectName));
}

void MainWindow::onNew() {
    if (dirty) {
        auto r = QMessageBox::question(this, "未保存", "当前工程未保存，确定新建？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }
    proj = Project();
    curFile.clear();
    dirty = false;
    seqPreviewMode = false;   // 新工程回到分镜编辑页
    setWindowTitle(QString("%1 - H3Hint").arg(proj.projectName));
    buildTree();
    selectItem("settings", -1);
}

void MainWindow::openProjectFile(const QString& f) {
    Project p;
    if (!p.load(f)) {
        QMessageBox::warning(this, "错误", "无法打开工程文件");
        return;
    }
    proj = p;
    curFile = f;
    dirty = false;
    seqPreviewMode = false;   // 打开工程回到分镜编辑页
    setWindowTitle(QString("%1 - H3Hint").arg(proj.projectName));
    buildTree();
    selectItem("settings", -1);
    addRecentFile(f);
}

void MainWindow::onOpen() {
    QString f = QFileDialog::getOpenFileName(this, "打开工程", proj.baseDir,
                                            "H3 工程 (*.h3json *.json)");
    if (f.isEmpty()) return;
    if (dirty) {
        auto r = QMessageBox::question(this, "未保存", "当前工程未保存，确定打开其它工程？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }
    openProjectFile(f);
}

void MainWindow::openRecent(const QString& f) {
    if (!QFileInfo::exists(f)) {
        QMessageBox::warning(this, "文件不存在", QString("找不到文件：\n%1").arg(f));
        appCfg.recentProjects.removeAll(f);
        appCfg.save(appCfgPath);
        refreshRecentMenu();
        return;
    }
    if (dirty) {
        auto r = QMessageBox::question(this, "未保存", "当前工程未保存，确定打开其它工程？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }
    openProjectFile(f);
}

void MainWindow::addRecentFile(const QString& f) {
    const QString abs = QFileInfo(f).absoluteFilePath();
    appCfg.recentProjects.removeAll(abs);
    appCfg.recentProjects.prepend(abs);
    while (appCfg.recentProjects.size() > 10) appCfg.recentProjects.removeLast();
    appCfg.save(appCfgPath);
    refreshRecentMenu();
}

void MainWindow::refreshRecentMenu() {
    if (!recentMenu) return;
    recentMenu->clear();
    if (appCfg.recentProjects.isEmpty()) {
        auto* a = recentMenu->addAction("(无)");
        a->setEnabled(false);
        return;
    }
    for (const QString& f : appCfg.recentProjects) {
        auto* a = recentMenu->addAction(f);
        connect(a, &QAction::triggered, this, [this, f]() { openRecent(f); });
    }
    recentMenu->addSeparator();
    auto* clearAct = recentMenu->addAction("清除最近记录");
    connect(clearAct, &QAction::triggered, this, [this]() {
        appCfg.recentProjects.clear();
        appCfg.save(appCfgPath);
        refreshRecentMenu();
    });
}

void MainWindow::onSave() {
    if (curFile.isEmpty()) { onSaveAs(); return; }
    if (!proj.save(curFile)) {
        QMessageBox::warning(this, "错误", "保存失败");
        return;
    }
    dirty = false;
    setWindowTitle(QString("%1 - H3Hint").arg(proj.projectName));
    addRecentFile(curFile);
}

void MainWindow::onSaveAs() {
    QString f = QFileDialog::getSaveFileName(this, "保存工程", proj.baseDir + "/project.h3json",
                                            "H3 工程 (*.h3json)");
    if (f.isEmpty()) return;
    if (!proj.save(f)) {
        QMessageBox::warning(this, "错误", "保存失败");
        return;
    }
    curFile = f;
    dirty = false;
    setWindowTitle(QString("%1 - H3Hint").arg(proj.projectName));
    addRecentFile(f);
}

void MainWindow::onGenerate() {
    GenDialog dlg(proj, appCfg, this);
    dlg.exec();
}

void MainWindow::onAppSettings() {
    AppSettingsDialog dlg(appCfg, this);
    // 「应用」：保存并即时切换主题，但不关闭对话框
    connect(&dlg, &AppSettingsDialog::applied, this, [this]() {
        appCfg.save(appCfgPath);
        Theme::apply(qApp, appCfg.theme);
    });
    if (dlg.exec() == QDialog::Accepted) {
        appCfg.save(appCfgPath);
        Theme::apply(qApp, appCfg.theme);
    }
}

void MainWindow::onLoadExample() {
    if (dirty) {
        auto r = QMessageBox::question(this, "未保存", "当前工程未保存，载入示例将覆盖，继续？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }
    Project p;
    p.projectName = "示例：神庭快递招新";
    p.settings.styleConstraint = "全程维持：中式二次元动画、新海诚式极致光影、精致写实向背景美术、光线粒子与尘埃浮动、暖色调高对比。室外和窗外保持漆黑，无任何室外光源，没有太阳。";

    Subject s1; s1.id = "Subject 1"; s1.name = "昭月月"; s1.alias = "本体"; s1.desc = "短发妹子"; s1.qId = "Subject 2";
    AudioRef a1; a1.id = "Audio 1"; a1.name = "昭月月音色参考"; a1.desc = "昭月月音色参考"; s1.audios.append(a1);

    Subject s2; s2.id = "Subject 2"; s2.name = "昭月月"; s2.alias = "Q版小人"; s2.isQ = true; s2.qId = "Subject 1";
    AudioRef a2; a2.id = "Audio 2"; a2.name = "Q版小人音色参考"; a2.desc = "Q版小人音色参考"; s2.audios.append(a2);

    p.subjects << s1 << s2;

    Scene sc; sc.id = "Scene 1";
    sc.desc = "夜晚的房间布置，房间，床头灯，床，柜子，矿泉水瓶。";
    p.scenes << sc;

    Prop pr; pr.id = "Prop 1";
    pr.desc = "招新传单，提供传单的画面信息，其中正面写着“神庭快递” 与 “高薪诚聘”，背面是空白。";
    p.props << pr;

    Sequence seq;
    seq.name = "招新片段";
    seq.lipConstraint = "全程只有Q版小人<Subject 2>在说话，画面中其余人物始终闭嘴、嘴唇完全不动、不发出任何声音。";
    seq.cast << "Subject 1" << "Subject 2";
    seq.scenes << "Scene 1";
    seq.props << "Prop 1";

    Shot sh1; sh1.id = "Shot 1"; sh1.label = "床头灯下";
    SubShot ss1;
    ss1.caption = "<Subject 1>手里拿着<Prop 1>（招新传单），画面中是<Prop 1>的背面，显示空白，低头看着。<Subject 2>凭空出现在桌面上，镜头靠近<Subject 2>，";
    Speech sp1; sp1.subject = "Subject 2"; sp1.s2 = true; sp1.audio = "Audio 2";
    sp1.lead = "担心的"; sp1.text = "包吃包住，怎么越看越像缅北...";
    ss1.speeches << sp1;
    SubShot ss2; ss2.caption = "<Subject 2>无奈摆了摆手，凭空消失。";
    sh1.subShots << ss1 << ss2;

    Shot sh2; sh2.id = "Shot 2"; sh2.label = "熄灯睡觉";
    SubShot ss3;
    ss3.caption = "镜头靠近<Subject 1>，<Subject 1>放下手中传单，躺在灯光下枕头上，爬进被窝，倒头就睡。“啪”的一声关闭灯光，画面变得黑暗。";
    sh2.subShots << ss3;

    seq.shots << sh1 << sh2;
    p.sequences << seq;

    proj = p;
    curFile.clear();
    dirty = false;
    setWindowTitle(QString("%1 - H3Hint").arg(proj.projectName));
    buildTree();
    selectItem("settings", -1);
}

void MainWindow::selectShotNode(int seqIdx, int shotIdx) {
    QTreeWidgetItem* root = tree->topLevelItem(0);   // 工程根
    if (!root) return;
    for (int i = 0; i < root->childCount(); ++i) {
        auto* cat = root->child(i);
        if (cat->data(0, Qt::UserRole).toString() == "sequences_header") {
            if (seqIdx < cat->childCount()) {
                auto* seqIt = cat->child(seqIdx);
                if (shotIdx < seqIt->childCount()) {
                    tree->setCurrentItem(seqIt->child(shotIdx));
                    return;
                }
            }
        }
    }
}

// ---------------- 资源导入 / 导出 ----------------
void MainWindow::doExportAssets(int kind) {
    const bool all  = (kind < 0);
    const bool doS  = all || kind == 0;   // 角色
    const bool doSc = all || kind == 1;   // 场景
    const bool doP  = all || kind == 2;   // 道具

    QJsonObject root;
    root["format"]  = "H3Hint.assets";
    root["version"] = 1;
    if (doS) {
        QJsonArray a;
        for (const auto& s : proj.subjects) a.append(s.toJson(proj.baseDir));
        root["subjects"] = a;
    }
    if (doSc) {
        QJsonArray a;
        for (const auto& s : proj.scenes) a.append(s.toJson(proj.baseDir));
        root["scenes"] = a;
    }
    if (doP) {
        QJsonArray a;
        for (const auto& s : proj.props) a.append(s.toJson(proj.baseDir));
        root["props"] = a;
    }

    const QString what = all ? "全部资源" : doS ? "角色" : doSc ? "场景" : "道具";
    QString fn = QFileDialog::getSaveFileName(this, "导出" + what,
                                             what + ".h3assets.json",
                                             "H3 资源 (*.json);;所有文件 (*)");
    if (fn.isEmpty()) return;
    if (!fn.endsWith(".json", Qt::CaseInsensitive)) fn += ".json";

    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件：\n" + fn);
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    statusBar()->showMessage(
        QString("已导出%1（角色 %2 / 场景 %3 / 道具 %4）到 %5")
            .arg(what)
            .arg(doS ? proj.subjects.size() : 0)
            .arg(doSc ? proj.scenes.size() : 0)
            .arg(doP ? proj.props.size() : 0)
            .arg(fn), 8000);
}

void MainWindow::doImportAssets() {
    QString fn = QFileDialog::getOpenFileName(this, "导入资源", QString(),
                                             "H3 资源 (*.json);;所有文件 (*)");
    if (fn.isEmpty()) return;

    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "导入失败", "无法读取文件：\n" + fn);
        return;
    }
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (!doc.isObject()) {
        QMessageBox::warning(this, "导入失败",
            "不是有效的资源文件：\n" + perr.errorString());
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray aSub = root["subjects"].toArray();
    const QJsonArray aSce = root["scenes"].toArray();
    const QJsonArray aPro = root["props"].toArray();
    if (aSub.isEmpty() && aSce.isEmpty() && aPro.isEmpty()) {
        QMessageBox::information(this, "导入资源", "文件中没有可导入的角色 / 场景 / 道具。");
        return;
    }

    auto maxNum = [](const QString& prefix, auto&& list) {
        int mx = 0;
        QRegularExpression re("^" + QRegularExpression::escape(prefix) + "\\s+(\\d+)$");
        for (const auto& x : list) {
            auto m = re.match(x.id);
            if (m.hasMatch()) mx = qMax(mx, m.captured(1).toInt());
        }
        return mx;
    };
    auto hasSub = [this](const QString& id) {
        for (const auto& s : proj.subjects) if (s.id == id) return true;
        return false;
    };
    auto hasSce = [this](const QString& id) {
        for (const auto& s : proj.scenes) if (s.id == id) return true;
        return false;
    };
    auto hasPro = [this](const QString& id) {
        for (const auto& s : proj.props) if (s.id == id) return true;
        return false;
    };

    int nSub = 0, nSce = 0, nPro = 0;

    QHash<QString, QString> subMap;   // 旧 id → 新 id（冲突重编号时）
    for (const auto& v : aSub) {
        Subject s;
        s.fromJson(v.toObject(), proj.baseDir);
        const QString oldId = s.id;
        while (hasSub(s.id)) s.id = QString("Subject %1").arg(maxNum("Subject", proj.subjects) + 1);
        if (oldId != s.id) subMap[oldId] = s.id;
        proj.subjects.append(s);
        ++nSub;
    }
    if (!subMap.isEmpty()) {
        for (auto& s : proj.subjects)
            if (subMap.contains(s.qId)) s.qId = subMap.value(s.qId);
    }

    for (const auto& v : aSce) {
        Scene s;
        s.fromJson(v.toObject(), proj.baseDir);
        while (hasSce(s.id)) s.id = QString("Scene %1").arg(maxNum("Scene", proj.scenes) + 1);
        proj.scenes.append(s);
        ++nSce;
    }

    for (const auto& v : aPro) {
        Prop p;
        p.fromJson(v.toObject(), proj.baseDir);
        while (hasPro(p.id)) p.id = QString("Prop %1").arg(maxNum("Prop", proj.props) + 1);
        proj.props.append(p);
        ++nPro;
    }

    markDirty();
    buildTree();
    statusBar()->showMessage(
        QString("已导入：角色 %1 / 场景 %2 / 道具 %3（来自 %4）")
            .arg(nSub).arg(nSce).arg(nPro).arg(fn), 8000);
    QMessageBox::information(this, "导入资源",
        QString("导入完成：角色 %1 个，场景 %2 个，道具 %3 个。").arg(nSub).arg(nSce).arg(nPro));
}

void MainWindow::onAbout() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("关于 H3Hint");
    dlg->setMinimumWidth(440);

    auto* v = new QVBoxLayout(dlg);

    auto* title = new QLabel("<b>H3Hint — MiniMax H3 提示词工程编辑器</b>");
    title->setWordWrap(true);
    v->addWidget(title);

    auto* desc = new QLabel("用 C++ / Qt 6 编写。工程以 JSON 配置，支持角色四视图参考图、场景、道具、"
                            "音色音频资源管理，分镜与台词编辑，并一键生成 H3 六段结构提示词。");
    desc->setWordWrap(true);
    v->addWidget(desc);

    auto* info = new QGroupBox("作者 / 联系方式");
    auto* fl = new QFormLayout(info);
    auto addRow = [&](const QString& label, const QString& value) {
        auto* le = new QLineEdit(value);
        le->setReadOnly(true);
        fl->addRow(label, le);
    };
    addRow("组织：", "中国人民共同富裕实行研究会");
    addRow("B站名：", "实行宫周慧");
    addRow("B站 ID：", "bili_94689949015");
    addRow("QQ群：", "1104274724");
    v->addWidget(info);

    auto* h = new QHBoxLayout;
    h->addStretch(1);
    auto* ok = new QPushButton("确定");
    h->addWidget(ok);
    v->addLayout(h);
    connect(ok, &QPushButton::clicked, dlg, &QDialog::accept);

    dlg->exec();
}
