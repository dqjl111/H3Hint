#include "previewdock.h"
#include "project.h"
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
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QResizeEvent>
#include <QLabel>
#include <QSplitter>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QMenu>
#include <QFileDialog>
#include <QFile>
#include <QIODevice>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QFont>
#include <algorithm>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QTextCharFormat>

// 结构化源数据表：宽度变化时按内容自动重算行高，长句可完整换行显示
class SourceTable : public QTableWidget {
public:
    explicit SourceTable(QWidget* parent = nullptr) : QTableWidget(parent) {}
protected:
    void resizeEvent(QResizeEvent* e) override {
        QTableWidget::resizeEvent(e);
        // 仅当可视宽度变化时才按内容重算行高；宽度不变（如手动拖动行高）则不动，
        // 以免覆盖用户手动调整过的行高
        int w = viewport()->width();
        if (w == lastWidth_) return;
        lastWidth_ = w;
        if (relayouting_) return;
        relayouting_ = true;
        // 延迟到宽度落定后再按内容重算行高，避免拖动分隔条时用旧宽度裁掉内容
        QTimer::singleShot(0, this, [this]() {
            setUpdatesEnabled(false);
            resizeRowsToContents();
            setUpdatesEnabled(true);
            relayouting_ = false;
        });
    }
private:
    bool relayouting_ = false;
    int lastWidth_ = -1;
};

// 提示词来源标注（以“// ←”开头的行）用醒目的亮橙加粗 + 淡底高亮
class PromptHighlighter : public QSyntaxHighlighter {
public:
    explicit PromptHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) {}
protected:
    void highlightBlock(const QString& text) override {
        if (text.trimmed().startsWith("// ←")) {
            QTextCharFormat fmt;
            fmt.setForeground(QColor(255, 138, 0));      // 亮橙，深浅主题都醒目
            fmt.setFontWeight(QFont::Bold);
            fmt.setBackground(QColor(255, 138, 0, 38)); // 半透明淡底，进一步区分
            setFormat(0, text.length(), fmt);
        }
    }
};

PreviewDock::PreviewDock(Project* proj, AppConfig* cfg, int sequenceIndex, QWidget* parent)
    : QWidget(parent), proj_(proj), cfg_(cfg), sequenceIndex_(sequenceIndex) {
    auto* v = new QVBoxLayout(this);

    // ---- 顶部“生成选项”分组（可折叠隐藏） ----
    optGroup_ = new QGroupBox("生成选项（画风 / 声音 / 音乐，取自应用设置；改动即预览）");
    auto* gv = new QVBoxLayout(optGroup_);
    lwStyle_ = new QListWidget;
    lwStyle_->setSelectionMode(QAbstractItemView::MultiSelection);
    gv->addWidget(lwStyle_);
    auto* sf = new QFormLayout;
    cbSound_ = new WheelSafeComboBox; cbSound_->setEditable(true);
    cbMusic_ = new WheelSafeComboBox; cbMusic_->setEditable(true);
    sf->addRow("背景环境声音 overall_soundscape", cbSound_);
    sf->addRow("背景音乐 non_diegetic_music", cbMusic_);
    gv->addLayout(sf);

    // ---- 下方内容（对照/标注开关、提示词编辑区、按钮） ----
    auto* rest = new QWidget;
    auto* restV = new QVBoxLayout(rest);
    restV->setContentsMargins(0, 0, 0, 0);

    // 对照模式开关 + 折叠“生成选项”按钮
    auto* tRow = new QHBoxLayout;
    chkCompare_ = new QCheckBox("对照模式（右侧显示结构化源数据）");
    chkAnnot_   = new QCheckBox("显示来源标注（编辑区内逐行注释）");
    chkCompare_->setChecked(cfg_->previewCompare);
    chkAnnot_->setChecked(cfg_->previewAnnot);
    btnOptToggle_ = new QPushButton("隐藏生成选项");
    tRow->addWidget(chkCompare_);
    tRow->addWidget(chkAnnot_);
    tRow->addStretch(1);
    tRow->addWidget(btnOptToggle_);
    restV->addLayout(tRow);

    noteLabel_ = new QLabel;
    noteLabel_->setWordWrap(true);
    noteLabel_->setStyleSheet("color:#8B95A7;");
    restV->addWidget(noteLabel_);

    // 左右分栏：左=可编辑提示词，右=结构化源数据（对照模式时显示）
    auto* split = new QSplitter(Qt::Horizontal);
    editor_ = new QPlainTextEdit;
    editor_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor_->setMinimumHeight(180);
    QFont mono("Consolas");
    if (mono.family() != "Consolas") mono = QFont("Courier New");
    mono.setPointSize(10);
    editor_->setFont(mono);
    new PromptHighlighter(editor_->document());   // 来源标注行（// ←）显示灰色
    split->addWidget(editor_);

    srcTree_ = new SourceTable(this);
    srcTree_->setColumnCount(2);
    srcTree_->setHorizontalHeaderLabels(QStringList() << "层级" << "内容");
    srcTree_->setVisible(false);
    // 结构化数据以表格呈现：内容列自动换行并按内容自适应行高
    srcTree_->setWordWrap(true);
    srcTree_->setTextElideMode(Qt::ElideNone);
    // 表格只读显示：双击用于跳转镜头，选中单元格便于整格复制（不做单元格内编辑）
    srcTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    srcTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);  // 允许选中以便复制
    srcTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    // 双击某行 → 跳转到该行所属镜头的编辑页
    connect(srcTree_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* it = srcTree_->item(row, 0)) {
            int k = it->data(Qt::UserRole).toInt();
            if (k >= 0) emit requestOpenShot(k);
        }
    });
    srcTree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    srcTree_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 显示纵向表头，允许用户手动拖动调整每一行的高度
    srcTree_->verticalHeader()->setVisible(true);
    srcTree_->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    srcTree_->verticalHeader()->setDefaultSectionSize(22);
    srcTree_->horizontalHeader()->setStretchLastSection(true);
    srcTree_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    srcTree_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    split->addWidget(srcTree_);

    // 复制表格内容：只复制真正被选中的单元格；按行聚合、行间换行，写入剪贴板
    auto copyTable = [this](bool all) {
        QStringList out;
        for (int r = 0; r < srcTree_->rowCount(); ++r) {
            QStringList cells;
            for (int c = 0; c < srcTree_->columnCount(); ++c) {
                auto* it = srcTree_->item(r, c);
                if (!it) continue;
                if (all || it->isSelected()) cells << it->text();
            }
            if (!cells.isEmpty()) out << cells.join("\t");
        }
        if (!out.isEmpty()) QApplication::clipboard()->setText(out.join("\n"));
    };
    connect(srcTree_, &QWidget::customContextMenuRequested, this, [this, copyTable](const QPoint& p) {
        QMenu menu;
        menu.addAction("复制选中", this, [copyTable]() { copyTable(false); });
        menu.addAction("复制全部", this, [copyTable]() { copyTable(true); });
        menu.exec(srcTree_->viewport()->mapToGlobal(p));
    });
    auto* copySc = new QShortcut(QKeySequence::Copy, srcTree_);
    connect(copySc, &QShortcut::activated, this, [copyTable]() { copyTable(false); });

    auto* toolBar = new QWidget;
    toolBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* h = new QHBoxLayout(toolBar);
    h->setContentsMargins(0, 0, 0, 0);
    chkAuto_ = new QCheckBox("自动刷新");
    chkAuto_->setChecked(cfg_->previewAutoRefresh);
    auto* btnRefresh = new QPushButton("刷新");
    btnUpdate_ = new QPushButton("更新配置");
    btnRestore_ = new QPushButton("恢复自动生成");
    auto* btnCopy = new QPushButton("复制");
    auto* btnSave = new QPushButton("保存 .txt");
    h->addWidget(chkAuto_);
    h->addWidget(btnRefresh);
    h->addWidget(btnUpdate_);
    h->addWidget(btnRestore_);
    h->addWidget(btnCopy);
    h->addWidget(btnSave);
    h->addStretch(1);

    // ---- 竖向分隔条：上=提示词/对照区（可拖动整体上下调整高度），下=操作按钮 ----
    auto* cmpSplit = new QSplitter(Qt::Vertical);
    cmpSplit->setHandleWidth(7);
    cmpSplit->addWidget(split);
    cmpSplit->addWidget(toolBar);
    cmpSplit->setStretchFactor(0, 1);
    cmpSplit->setStretchFactor(1, 0);
    cmpSplit->setCollapsible(0, false);
    cmpSplit->setCollapsible(1, false);
    // 记住用户手动调过的对照区高度
    if (!cfg_->previewCompareSplitSizes.isEmpty())
        QTimer::singleShot(0, this, [this, cmpSplit]() { cmpSplit->setSizes(cfg_->previewCompareSplitSizes); });
    connect(cmpSplit, &QSplitter::splitterMoved, this, [this, cmpSplit](int, int) {
        cfg_->previewCompareSplitSizes = cmpSplit->sizes();
    });
    restV->addWidget(cmpSplit, 1);

    // ---- 竖向分隔条：上=生成选项，下=预览/对照（可拖动手柄折叠上方） ----
    optSplit_ = new QSplitter(Qt::Vertical);
    optSplit_->addWidget(optGroup_);
    optSplit_->addWidget(rest);
    optSplit_->setStretchFactor(0, 0);
    optSplit_->setStretchFactor(1, 1);
    v->addWidget(optSplit_, 1);

    // 折叠 / 展开“生成选项”
    optGroup_->setVisible(!cfg_->previewOptCollapsed);
    btnOptToggle_->setText(cfg_->previewOptCollapsed ? "显示生成选项" : "隐藏生成选项");
    // 恢复上次拖拽后的分隔条尺寸（未折叠时）
    if (!cfg_->previewOptCollapsed && !cfg_->previewOptSplitSizes.isEmpty()) {
        QTimer::singleShot(0, this, [this]() { optSplit_->setSizes(cfg_->previewOptSplitSizes); });
    }
    connect(optSplit_, &QSplitter::splitterMoved, this, [this](int, int) {
        if (!cfg_->previewOptCollapsed) cfg_->previewOptSplitSizes = optSplit_->sizes();
    });
    connect(btnOptToggle_, &QPushButton::clicked, this, [this]() {
        bool visible = optGroup_->isVisible();
        if (visible) {
            cfg_->previewOptSplitSizes = optSplit_->sizes();   // 记住折叠前尺寸
            cfg_->previewOptCollapsed = true;
            optGroup_->setVisible(false);
            btnOptToggle_->setText("显示生成选项");
        } else {
            cfg_->previewOptCollapsed = false;
            optGroup_->setVisible(true);
            if (!cfg_->previewOptSplitSizes.isEmpty())
                optSplit_->setSizes(cfg_->previewOptSplitSizes);
            btnOptToggle_->setText("隐藏生成选项");
        }
    });

    refreshOptions();

    // 整工程预览没有单一分镜可覆盖/对照，禁用相关控件
    bool canOverride = sequenceIndex_ >= 0 && sequenceIndex_ < proj_->sequences.size();
    btnUpdate_->setEnabled(canOverride);
    btnRestore_->setEnabled(canOverride);
    chkCompare_->setEnabled(canOverride);
    chkAnnot_->setEnabled(canOverride);

    connect(lwStyle_, &QListWidget::itemSelectionChanged, this, &PreviewDock::onOptionsChanged);
    connect(cbSound_, &QComboBox::currentTextChanged, this, &PreviewDock::onOptionsChanged);
    connect(cbMusic_, &QComboBox::currentTextChanged, this, &PreviewDock::onOptionsChanged);
    connect(chkAuto_, &QCheckBox::toggled, this, [this](bool on) {
        cfg_->previewAutoRefresh = on;
        if (on) refresh();
    });
    connect(btnRefresh, &QPushButton::clicked, this, &PreviewDock::refresh);
    connect(editor_, &QPlainTextEdit::textChanged, this, &PreviewDock::onTextChanged);
    connect(btnUpdate_, &QPushButton::clicked, this, &PreviewDock::onUpdateConfig);
    connect(btnRestore_, &QPushButton::clicked, this, &PreviewDock::onRestoreAuto);
    connect(chkCompare_, &QCheckBox::toggled, this, &PreviewDock::onToggleCompare);
    connect(chkAnnot_, &QCheckBox::toggled, this, &PreviewDock::onToggleAnnot);
    connect(btnCopy, &QPushButton::clicked, [this]() {
        QApplication::clipboard()->setText(currentCleanText());
    });
    connect(btnSave, &QPushButton::clicked, [this]() {
        QString f = QFileDialog::getSaveFileName(nullptr, "保存提示词",
                                                 proj_->baseDir + "/prompt.txt", "Text (*.txt)");
        if (!f.isEmpty()) {
            QFile file(f);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(currentCleanText().toUtf8());
                file.close();
            }
        }
    });
    refresh();

    // 构造时已按 AppConfig 设置了勾选框，但勾选框不会触发实际的面板效果，
    // 这里手动应用一次，使面板状态（对照表显隐 / 来源标注文本）与按键状态一致
    if (cfg_->previewCompare) onToggleCompare(true);
    if (cfg_->previewAnnot) onToggleAnnot(true);
}

GenOptions PreviewDock::currentOptions() const {
    GenOptions opt;
    for (auto* it : lwStyle_->selectedItems()) opt.styles.append(it->text());
    opt.soundscape = cbSound_->currentText();
    opt.music = cbMusic_->currentText();
    return opt;
}

void PreviewDock::refreshOptions() {
    bool hasSel = !cfg_->previewStyleSel.isEmpty();
    lwStyle_->clear();
    for (int i = 0; i < cfg_->commonStyles.size(); ++i) {
        auto* it = new QListWidgetItem(cfg_->commonStyles[i], lwStyle_);
        it->setSelected(hasSel ? cfg_->previewStyleSel.contains(i) : true);
    }
    cbSound_->clear(); cbSound_->addItems(cfg_->soundscapes);
    cbMusic_->clear(); cbMusic_->addItems(cfg_->musics);
    int i = cbSound_->findText(cfg_->previewSound);
    cbSound_->setCurrentIndex(i >= 0 ? i : (cbSound_->count() ? 0 : -1));
    i = cbMusic_->findText(cfg_->previewMusic);
    cbMusic_->setCurrentIndex(i >= 0 ? i : (cbMusic_->count() ? 0 : -1));
}

void PreviewDock::onOptionsChanged() {
    // 记住当前生成选项的选择，跨分镜切换保持一致
    cfg_->previewStyleSel.clear();
    for (int i = 0; i < lwStyle_->count(); ++i)
        if (lwStyle_->item(i)->isSelected()) cfg_->previewStyleSel.append(i);
    cfg_->previewSound = cbSound_->currentText();
    cfg_->previewMusic = cbMusic_->currentText();
    if (chkAuto_->isChecked()) refresh();
}

void PreviewDock::onTextChanged() {
    dirty_ = true;
}

QString PreviewDock::validateNumbering(const QString& text) const {
    // 检查各类局部编号是否“从 1 开始、连续无缺”。捕获组即为编号数字。
    auto check = [&](const QString& label, const QString& pat) -> QString {
        QRegularExpression re(pat);
        QSet<int> nums;
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            int n = it.next().captured(1).toInt();
            if (n > 0) nums.insert(n);
        }
        if (nums.isEmpty()) return QString();
        int mx = -1;
        for (int n : nums) if (n > mx) mx = n;
        QStringList present;
        for (int n : nums) present.append(QString::number(n));
        for (int i = 1; i <= mx; ++i) {
            if (!nums.contains(i)) {
                return QString("%1 编号不连续：应为从 1 开始的连续编号（当前出现 %2，缺少 %3）。")
                    .arg(label).arg(present.join(", ")).arg(i);
            }
        }
        return QString();
    };
    QString e = check("<Subject N>", "<(Subject)\\s+(\\d+)>");  if (!e.isEmpty()) return e;
    e = check("<Audio N>",   "<(Audio)\\s+(\\d+)>");            if (!e.isEmpty()) return e;
    e = check("<Picture N>", "<(Picture)\\s+(\\d+)>");           if (!e.isEmpty()) return e;
    e = check("[Shot N]",    "\\[Shot\\s+(\\d+)\\]");            if (!e.isEmpty()) return e;
    return QString();
}

QString PreviewDock::currentCleanText() const {
    QStringList lines = editor_->toPlainText().split('\n');
    lines.erase(std::remove_if(lines.begin(), lines.end(),
        [](const QString& l) { return l.trimmed().startsWith("// ←"); }),
        lines.end());
    return lines.join('\n');
}

void PreviewDock::buildSourceTable() {
    srcTree_->clearContents();
    srcTree_->setRowCount(0);
    if (sequenceIndex_ < 0 || sequenceIndex_ >= proj_->sequences.size()) return;
    const auto& seq = proj_->sequences[sequenceIndex_];
    auto subjectName = [this](const QString& id) -> QString {
        for (const auto& s : proj_->subjects)
            if (s.id == id) return s.name.isEmpty() ? id : s.name;
        return id;
    };
    auto addRow = [&](const QString& a, const QString& b, int shotIdx) {
        int r = srcTree_->rowCount();
        srcTree_->insertRow(r);
        auto* i0 = new QTableWidgetItem(a);
        auto* i1 = new QTableWidgetItem(b);
        i0->setTextAlignment(Qt::AlignTop);
        i1->setTextAlignment(Qt::AlignTop);
        // 记录该行所属镜头序号，供双击跳转到镜头编辑页
        i0->setData(Qt::UserRole, shotIdx);
        i1->setData(Qt::UserRole, shotIdx);
        srcTree_->setItem(r, 0, i0);
        srcTree_->setItem(r, 1, i1);
    };
    int speechNo = 0;   // 整段提示词内的句子序号：(S1)、(S2)…（跨镜头连续递增）
    for (int k = 0; k < seq.shots.size(); ++k) {
        int shotNo = k + 1;
        QString lbl = seq.shots[k].label.trimmed();
        addRow(QString("镜头 %1%2").arg(shotNo).arg(lbl.isEmpty() ? "" : ("：" + lbl)), QString(), k);
        int ssNo = 0;
        for (const auto& ss : seq.shots[k].subShots) {
            ++ssNo;
            addRow("  段落 " + QString::number(ssNo), QString(), k);
            QString cap = ss.caption.trimmed();
            if (!cap.isEmpty()) {
                // 画面描述本身可能已带“画面：”前缀，避免重复
                if (!cap.startsWith("画面：") && !cap.startsWith("画面:")) cap.prepend("画面：");
                addRow("    画面", cap, k);
            }
            for (const auto& sp : ss.speeches) {
                QString who = subjectName(sp.subject);
                QString txt = sp.text.trimmed();
                addRow("    台词（" + who + "）(S" + QString::number(++speechNo) + ")", txt, k);
            }
        }
    }
    // 列宽确定后按内容重算行高，避免长句被裁切
    srcTree_->resizeRowsToContents();
    QTimer::singleShot(0, this, [this]() {
        if (srcTree_ && srcTree_->isVisible()) srcTree_->resizeRowsToContents();
    });
}

void PreviewDock::onToggleCompare(bool on) {
    cfg_->previewCompare = on;
    srcTree_->setVisible(on);
    if (on) {
        buildSourceTable();
        // 分栏在下一轮布局才把宽度分配给右侧表，届时再按新宽度重算行高
        QTimer::singleShot(0, this, [this]() {
            if (srcTree_ && srcTree_->isVisible()) srcTree_->resizeRowsToContents();
        });
    }
}

void PreviewDock::onToggleAnnot(bool on) {
    cfg_->previewAnnot = on;
    if (on) {
        // 进入对照：按当前工程重新生成带注释文本（会丢弃未保存的手动编辑）
        dirty_ = false;
        refresh();
    } else {
        // 退出对照：仅剔除注释行，保留其他手动编辑
        QSignalBlocker b(editor_);
        editor_->setPlainText(currentCleanText());
    }
}

void PreviewDock::onUpdateConfig() {
    if (sequenceIndex_ < 0 || sequenceIndex_ >= proj_->sequences.size()) return;
    QString clean = currentCleanText();
    QString err = validateNumbering(clean);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, "编号错误，未保存", err);
        return;
    }
    proj_->sequences[sequenceIndex_].promptOverride = clean;
    dirty_ = false;
    emit projectEdited();
    refresh();   // 重写显示（覆盖后注释模式自动失效）
    QMessageBox::information(this, "已更新",
        "已将该提示词存为「分镜自定义提示词」，预览与导出将优先使用它。\n"
        "原段落 / 台词等结构化数据不变；点击「恢复自动生成」可清除覆盖。");
}

void PreviewDock::onRestoreAuto() {
    if (sequenceIndex_ < 0 || sequenceIndex_ >= proj_->sequences.size()) return;
    proj_->sequences[sequenceIndex_].promptOverride.clear();
    dirty_ = false;
    emit projectEdited();
    refresh();
}

void PreviewDock::refresh() {
    int idx = sequenceIndex_;
    if (idx < 0 || idx >= proj_->sequences.size()) {
        if (dirty_) return;
        QString text = generatePrompt(*proj_, currentOptions());
        QSignalBlocker b(editor_);
        editor_->setPlainText(text);
        noteLabel_->setText("整工程预览：编辑后不可逐分镜覆盖保存；请打开单个分镜预览以启用“更新配置”。");
        return;
    }
    if (dirty_) return;   // 不覆盖尚未保存的手动编辑

    GenOptions opt = currentOptions();
    const auto& seq = proj_->sequences[idx];
    QString override = seq.promptOverride;
    bool hasOverride = !override.trimmed().isEmpty();
    QString shown;
    if (chkAnnot_->isChecked() && !hasOverride)
        shown = generateAnnotatedSequencePrompt(*proj_, seq, opt);
    else
        shown = hasOverride ? override : generateSequencePrompt(*proj_, seq, opt);

    {
        QSignalBlocker b(editor_);
        editor_->setPlainText(shown);
    }

    if (hasOverride)
        noteLabel_->setText("● 当前使用「自定义提示词覆盖」（非自动生成）。编辑后点「更新配置」覆盖保存，或点「恢复自动生成」清除。");
    else if (chkAnnot_->isChecked())
        noteLabel_->setText("灰色的 “// ← 来源：” 行为自动标注，保存时会自动剔除，不影响最终提示词。");
    else
        noteLabel_->setText("提示：勾选「显示来源标注」可在每行前显示其对应的结构化数据来源；勾选「对照模式」可在右侧查看结构化源数据。");

    if (srcTree_->isVisible()) buildSourceTable();
}

bool PreviewDock::autoRefresh() const {
    return chkAuto_->isChecked();
}
