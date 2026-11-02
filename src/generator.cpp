#include "generator.h"
#include "project.h"

#include <QRegularExpression>
#include <QStringList>
#include <QSet>
#include <QHash>
#include <algorithm>

namespace {

// 在文本中收集某种标签 <Tag N> 出现的编号（按出现顺序、去重）
QList<int> collectTagNumbers(const QString& text, const QString& tag) {
    QList<int> res;
    QRegularExpression re("<" + QRegularExpression::escape(tag) + "\\s+(\\d+)>");
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        int n = it.next().captured(1).toInt();
        if (!res.contains(n)) res.append(n);
    }
    return res;
}

const Subject* findSubject(const Project& p, const QString& id) {
    for (const auto& s : p.subjects) if (s.id == id) return &s;
    return nullptr;
}
const Scene* findScene(const Project& p, const QString& id) {
    for (const auto& s : p.scenes) if (s.id == id) return &s;
    return nullptr;
}
const Prop* findProp(const Project& p, const QString& id) {
    for (const auto& s : p.props) if (s.id == id) return &s;
    return nullptr;
}

} // namespace

// 内部统一用 LineInfo（文本 + 来源标注）构建，便于生成“带来源标注”的对照版本。
using LineInfo = QPair<QString, QString>;  // first=提示词文本, second=来源标注(空=无)

static QList<LineInfo> buildSequenceLines(const Project& p, const Sequence& seq, const GenOptions& opt) {
    QString style;
    for (const auto& x : opt.styles) if (!x.trimmed().isEmpty()) style += x + " ";
    style = style.trimmed();

    // ---- 收集本分镜内使用到的资源（只收项目里真实存在的） ----
    QList<QString> subjG, sceneG, propG, audioG;   // 全局 id，按分镜内出现/配置顺序
    QHash<QString, QSet<int>> subjShots, sceneShots, propShots, audioShots;
    auto addUniq = [](QList<QString>& l, const QString& v) { if (!l.contains(v)) l.append(v); };

    // 分镜显式配置的出场角色 / 场景 / 道具优先（决定编号顺序）
    for (const auto& id : seq.cast)   if (findSubject(p, id)) addUniq(subjG, id);
    for (const auto& id : seq.scenes) if (findScene(p, id))   addUniq(sceneG, id);
    for (const auto& id : seq.props)  if (findProp(p, id))    addUniq(propG, id);

    for (int k = 0; k < seq.shots.size(); ++k) {
        int shotNo = k + 1;
        for (const auto& ss : seq.shots[k].subShots) {
            for (int n : collectTagNumbers(ss.caption, "Subject")) {
                QString g = QString("Subject %1").arg(n);
                if (findSubject(p, g)) { addUniq(subjG, g); subjShots[g].insert(shotNo); }
            }
            for (int n : collectTagNumbers(ss.caption, "Scene")) {
                QString g = QString("Scene %1").arg(n);
                if (findScene(p, g)) { addUniq(sceneG, g); sceneShots[g].insert(shotNo); }
            }
            for (int n : collectTagNumbers(ss.caption, "Prop")) {
                QString g = QString("Prop %1").arg(n);
                if (findProp(p, g)) { addUniq(propG, g); propShots[g].insert(shotNo); }
            }
            for (int n : collectTagNumbers(ss.caption, "Audio")) {
                QString g = QString("Audio %1").arg(n);
                addUniq(audioG, g); audioShots[g].insert(shotNo);
            }
            for (const auto& sp : ss.speeches) {
                if (findSubject(p, sp.subject)) {
                    addUniq(subjG, sp.subject); subjShots[sp.subject].insert(shotNo);
                }
                // 仅当明确选择了音色时才纳入 <Audio>；空音频不应生成 使用<Audio N>
                if (!sp.audio.trimmed().isEmpty()) {
                    addUniq(audioG, sp.audio); audioShots[sp.audio].insert(shotNo);
                    for (const auto& s : p.subjects)
                        for (const auto& a : s.audios)
                            if (a.id == sp.audio) {
                                addUniq(subjG, s.id); subjShots[s.id].insert(shotNo);
                            }
                }
            }
        }
    }

    // 标记分镜配置但未被 caption 引用的资源，至少出现在 Shot 1
    if (!seq.shots.isEmpty()) {
        for (const auto& g : seq.cast)   if (!subjShots.contains(g))  subjShots[g].insert(1);
        for (const auto& g : seq.scenes) if (!sceneShots.contains(g)) sceneShots[g].insert(1);
        for (const auto& g : seq.props)  if (!propShots.contains(g))  propShots[g].insert(1);
    }

    // ---- 统一编号：人物 → 场景 → 道具，连续递增为 <Subject N>；图片编号也连续 ----
    QHash<QString, int> ls, la, picOf;
    int pc = 0, n = 1;
    for (const auto& g : subjG)  { ls[g] = n++; picOf[g] = ++pc; }
    for (const auto& g : sceneG) { ls[g] = n++; picOf[g] = ++pc; }
    for (const auto& g : propG)  { ls[g] = n++; picOf[g] = ++pc; }
    n = 1;
    for (const auto& g : audioG) { la[g] = n++; }

    // 把文本里的 <Subject/Scene/Prop N> 统一映射为 <Subject 局部编号>，<Audio N> 映射为 <Audio 局部编号>
    auto remap = [&](const QString& text) -> QString {
        QString r = text;
        QRegularExpression re("<(Subject|Scene|Prop|Audio)\\s+(\\d+)>");
        int pos = 0;
        while (true) {
            auto mm = re.match(r, pos);
            if (!mm.hasMatch()) break;
            const QString type = mm.captured(1);
            const QString g = type + " " + mm.captured(2);
            const bool isAudio = (type == "Audio");
            const int local = isAudio ? la.value(g, -1) : ls.value(g, -1);
            if (local > 0) {
                const QString repl = isAudio
                    ? QString("<Audio %1>").arg(local)
                    : QString("<Subject %1>").arg(local);
                r.replace(mm.capturedStart(0), mm.capturedLength(0), repl);
                pos = mm.capturedStart(0) + repl.length();
            } else {
                pos = mm.capturedEnd(0);
            }
        }
        return r;
    };

    auto shotList = [](const QSet<int>& s) {
        QList<int> v = s.values();
        std::sort(v.begin(), v.end());
        QStringList r; for (int x : v) r.append(QString("[Shot %1]").arg(x));
        return r.join("、");
    };

    QList<LineInfo> out;
    auto add = [&](const QString& t, const QString& src = QString()) { out.append({t, src}); };

    // ---- subject_definitions（人物 / 场景 / 道具 统一为 <Subject N>） ----
    add("subject_definitions:");
    for (const auto& g : subjG) {
        const auto* s = findSubject(p, g);
        if (!s) continue;
        QString head = s->isQ
            ? QString("<Subject %1> 是 <Picture %2> 中的人物%3的Q版内心形象")
                  .arg(ls[g]).arg(picOf[g]).arg(s->name)
            : QString("<Subject %1> 是 <Picture %2> 中的人物%3（%4）")
                  .arg(ls[g]).arg(picOf[g]).arg(s->name).arg(s->desc);
        head += "，四视图（单张参考图：左起面部特写、全身正面、全身侧面、全身背面）提供人物身份、外形、服装和姿态参考。";
        add(head, QString("角色：%1").arg(s->name));
    }
    for (const auto& g : sceneG) {
        const auto* sc = findScene(p, g);
        if (!sc) continue;
        add(QString("<Subject %1> 是 <Picture %2>，%3")
                .arg(ls[g]).arg(picOf[g]).arg(remap(sc->desc)),
            QString("场景：%1").arg(sc->label));
    }
    for (const auto& g : propG) {
        const auto* pr = findProp(p, g);
        if (!pr) continue;
        add(QString("<Subject %1> 是 <Picture %2>，%3")
                .arg(ls[g]).arg(picOf[g]).arg(remap(pr->desc)),
            QString("道具：%1").arg(pr->label));
    }
    for (const auto& g : audioG) {
        QString owner;
        QString aname = g;   // 来源标注优先显示音频名称
        for (const auto& s : p.subjects)
            for (const auto& a : s.audios)
                if (a.id == g) {
                    if (!a.name.trimmed().isEmpty()) aname = a.name.trimmed();
                    if (ls.contains(s.id))
                        owner = QString("<Subject %1>").arg(ls[s.id]);
                }
        add(QString("<Audio %1> 是%2的音色参考音频，提供音色、语速与发声习惯参考，仅作音色参考，不复制音频内容。")
                .arg(la[g]).arg(owner),
            QString("音色：%1").arg(aname));
    }

    // ---- summary ----
    add("summary:");
    add(QString("[reference generation + character reference (I2VA)] %1").arg(style));
    add("全程无BGM。");
    add("全程无字幕。");

    // ---- retention_analysis ----
    add("retention_analysis:");
    for (const auto& g : subjG) {
        if (!findSubject(p, g)) continue;
        add(QString("<Subject %1>（出现在 %2）：fully_preserved - 人物身份、外形、服装和姿态按 <Picture %3>（四视图单张参考图：左起面部特写、全身正面、全身侧面、全身背面，外接图片输入） 保持。")
                .arg(ls[g]).arg(shotList(subjShots.value(g))).arg(picOf[g]),
            QString("角色：%1").arg(findSubject(p, g)->name));
    }
    for (const auto& g : sceneG) {
        if (!findScene(p, g)) continue;
        add(QString("<Subject %1>（出现在 %2）：fully_preserved - 场景布置与 <Picture %3> 保持。")
                .arg(ls[g]).arg(shotList(sceneShots.value(g))).arg(picOf[g]),
            QString("场景：%1").arg(findScene(p, g)->label));
    }
    for (const auto& g : propG) {
        if (!findProp(p, g)) continue;
        add(QString("<Subject %1>（出现在 %2）：fully_preserved - 道具画面按 <Picture %3> 保持。")
                .arg(ls[g]).arg(shotList(propShots.value(g))).arg(picOf[g]),
            QString("道具：%1").arg(findProp(p, g)->label));
    }
    for (const auto& g : audioG)
        add(QString("<Audio %1>：reference - 仅参考其音色与发声状态，不复制原音频信号。").arg(la[g]),
            QString("音色：%1").arg(g));
    if (!p.settings.styleConstraint.trimmed().isEmpty())
        add("画风约束：" + p.settings.styleConstraint.trimmed());
    if (!seq.lipConstraint.trimmed().isEmpty())
        add("嘴型约束：" + seq.lipConstraint.trimmed());

    // ---- detailed_description（本分镜下各镜头分段） ----
    add("detailed_description:");
    int ssNo = 0;
    int speechNo = 0;   // 整段提示词内的句子序号：(S1)、(S2)…（跨镜头连续递增）
    for (int k = 0; k < seq.shots.size(); ++k) {
        int shotNo = k + 1;
        QString shotLabel = seq.shots[k].label.trimmed();
        add(QString("[Shot %1]").arg(shotNo),
            shotLabel.isEmpty() ? QString("镜头%1").arg(shotNo)
                                : QString("镜头%1：%2").arg(shotNo).arg(shotLabel));
        for (const auto& ss : seq.shots[k].subShots) {
            ++ssNo;
            if (!ss.caption.trimmed().isEmpty())
                add(remap(ss.caption.trimmed()),
                    QString("镜头%1·段落%2 画面描述").arg(shotNo).arg(ssNo));
            for (const auto& sp : ss.speeches) {
                QString sTag = QString("(S%1)").arg(++speechNo);   // 按句子顺序编号
                QString note = sp.note.trimmed().isEmpty() ? "" : ("[备注：" + sp.note + "]");
                QString lead = sp.lead.trimmed();
                // 数据里未记录说话角色（空或存的是显示名）时，回退到本分镜出场角色，
                // 再退到本镜头首个出现的人物，避免生成 <Subject 0>
                QString subjId = sp.subject;
                if (!ls.contains(subjId)) {
                    for (const auto& cid : seq.cast)
                        if (ls.contains(cid)) { subjId = cid; break; }
                }
                if (!ls.contains(subjId) && !subjG.isEmpty())
                    subjId = subjG.first();
                int L = ls.value(subjId, 0);
                int A = la.value(sp.audio, 0);
                // 有音色时输出“使用<Audio N>”，无音色（A=0）则整段不带音频标记
                QString audioTag = (A > 0) ? QString("使用<Audio %1>").arg(A) : QString();
                add(QString("<Subject %1>%2%3%4%5说：“%6”")
                        .arg(L).arg(sTag).arg(note).arg(lead).arg(audioTag).arg(sp.text.trimmed()),
                    QString("镜头%1·段落%2 角色<Subject %3> 台词").arg(shotNo).arg(ssNo).arg(L));
            }
        }
    }

    // ---- 声音 ----
    add("overall_soundscape:");
    add(opt.soundscape.trimmed().isEmpty() ? "N/A" : opt.soundscape.trimmed());
    add("non_diegetic_music:");
    add(opt.music.trimmed().isEmpty() ? "N/A" : opt.music.trimmed());

    return out;
}

QString generateSequencePrompt(const Project& p, const Sequence& seq, const GenOptions& opt) {
    QStringList out;
    for (const auto& li : buildSequenceLines(p, seq, opt)) out.append(li.first);
    return out.join("\n");
}

// 带“来源标注”的对照版本：在每条由结构化数据生成的提示词行前插入一行灰色注释
// “// ← 来源：...”，用于编辑时对照原始结构化数据。注释行不参与编号校验、不保存。
QString generateAnnotatedSequencePrompt(const Project& p, const Sequence& seq, const GenOptions& opt) {
    QStringList out;
    for (const auto& li : buildSequenceLines(p, seq, opt)) {
        if (!li.second.isEmpty()) out.append("// ← 来源：" + li.second);
        out.append(li.first);
    }
    return out.join("\n");
}

QString generatePrompt(const Project& p, const GenOptions& opt) {
    QStringList blocks;
    for (int i = 0; i < p.sequences.size(); ++i) {
        const auto& seq = p.sequences[i];
        QString title = seq.name.trimmed().isEmpty()
            ? QString("分镜%1").arg(i + 1) : seq.name.trimmed();
        QString body = !seq.promptOverride.trimmed().isEmpty()
            ? seq.promptOverride
            : generateSequencePrompt(p, seq, opt);
        blocks.append(QString("========== 分镜：%1 ==========\n").arg(title) + body);
    }
    return blocks.join("\n\n");
}
