#include "project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QStandardPaths>

// ---------------- 路径换算 ----------------
QString Project::toRel(const QString& p, const QString& base) {
    if (p.isEmpty()) return "";
    if (QFileInfo(p).isAbsolute() && !base.isEmpty())
        return QDir(base).relativeFilePath(p);
    return p;
}

QString Project::toAbs(const QString& p, const QString& base) {
    if (p.isEmpty()) return "";
    if (QFileInfo(p).isAbsolute()) return p;
    if (base.isEmpty()) return p;
    return QDir(base).filePath(p);
}

// ---------------- AudioRef ----------------
QJsonObject AudioRef::toJson(const QString& base) const {
    QJsonObject o;
    o["id"]   = id;
    o["path"] = Project::toRel(path, base);
    o["desc"] = desc;
    return o;
}

void AudioRef::fromJson(const QJsonObject& o, const QString& base) {
    id   = o["id"].toString("Audio 1");
    path = Project::toAbs(o["path"].toString(), base);
    desc = o["desc"].toString();
}

// ---------------- Subject ----------------
QJsonObject Subject::toJson(const QString& base) const {
    QJsonObject o;
    o["id"]             = id;
    o["name"]           = name;
    o["alias"]          = alias;
    o["desc"]           = desc;
    o["is_q_version"]   = isQ;
    o["q_version_id"]   = qId;
    QJsonArray au;
    for (const auto& a : audios) au.append(a.toJson(base));
    o["view_set"] = Project::toRel(viewImage, base);
    o["audios"]   = au;
    return o;
}

void Subject::fromJson(const QJsonObject& o, const QString& base) {
    id    = o["id"].toString("Subject 1");
    name  = o["name"].toString();
    alias = o["alias"].toString();
    desc  = o["desc"].toString();
    isQ  = o["is_q_version"].toBool(false);
    qId  = o["q_version_id"].toString();
    viewImage = Project::toAbs(o["view_set"].toString(), base);
    audios.clear();
    for (const auto& a : o["audios"].toArray()) {
        AudioRef r; r.fromJson(a.toObject(), base); audios.append(r);
    }
}

// ---------------- Scene ----------------
QJsonObject Scene::toJson(const QString& base) const {
    QJsonObject o;
    o["id"]    = id;
    o["label"] = label;
    o["path"]  = Project::toRel(path, base);
    o["desc"]  = desc;
    return o;
}

void Scene::fromJson(const QJsonObject& o, const QString& base) {
    id    = o["id"].toString("Scene 1");
    label = o["label"].toString();
    path  = Project::toAbs(o["path"].toString(), base);
    desc  = o["desc"].toString();
}

// ---------------- Prop ----------------
QJsonObject Prop::toJson(const QString& base) const {
    QJsonObject o;
    o["id"]    = id;
    o["label"] = label;
    o["path"]  = Project::toRel(path, base);
    o["desc"]  = desc;
    return o;
}

void Prop::fromJson(const QJsonObject& o, const QString& base) {
    id    = o["id"].toString("Prop 1");
    label = o["label"].toString();
    path  = Project::toAbs(o["path"].toString(), base);
    desc  = o["desc"].toString();
}

// ---------------- Speech / SubShot / Shot ----------------
QJsonObject Speech::toJson() const {
    QJsonObject o;
    o["subject"] = subject;
    o["s2"]      = s2;
    o["note"]    = note;
    o["audio"]   = audio;
    o["lead"]    = lead;
    o["text"]    = text;
    return o;
}

void Speech::fromJson(const QJsonObject& o) {
    subject = o["subject"].toString();
    s2      = o["s2"].toBool(false);
    note    = o["note"].toString();
    audio   = o["audio"].toString();
    lead    = o["lead"].toString();
    text    = o["text"].toString();
}

QJsonObject SubShot::toJson() const {
    QJsonObject o;
    o["caption"] = caption;
    QJsonArray sp;
    for (const auto& s : speeches) sp.append(s.toJson());
    o["speeches"] = sp;
    return o;
}

void SubShot::fromJson(const QJsonObject& o) {
    caption = o["caption"].toString();
    speeches.clear();
    for (const auto& s : o["speeches"].toArray()) {
        Speech sp; sp.fromJson(s.toObject()); speeches.append(sp);
    }
}

QJsonObject Shot::toJson() const {
    QJsonObject o;
    o["id"]    = id;
    o["label"] = label;
    QJsonArray ss;
    for (const auto& s : subShots) ss.append(s.toJson());
    o["sub_shots"] = ss;
    return o;
}

void Shot::fromJson(const QJsonObject& o) {
    id = o["id"].toString("Shot 1");
    label = o["label"].toString();
    subShots.clear();
    for (const auto& s : o["sub_shots"].toArray()) {
        SubShot ss; ss.fromJson(s.toObject()); subShots.append(ss);
    }
    if (subShots.isEmpty()) subShots.append(SubShot());
}

// ---------------- Sequence (分镜) ----------------
QJsonObject Sequence::toJson() const {
    QJsonObject o;
    o["id"]    = id;
    o["name"]  = name;
    o["lip_constraint"] = lipConstraint;
    QJsonArray cs;
    for (const auto& c : cast) cs.append(c);
    o["cast"] = cs;
    QJsonArray scs;
    for (const auto& c : scenes) scs.append(c);
    o["scenes"] = scs;
    QJsonArray prs;
    for (const auto& c : props) prs.append(c);
    o["props"] = prs;
    // 已拆分镜头文件时只记录文件名；否则保留内联（兼容旧/样例）
    if (shotFiles.isEmpty()) {
        QJsonArray sh;
        for (const auto& s : shots) sh.append(s.toJson());
        o["shots"] = sh;
    }
    QJsonArray sf;
    for (const auto& f : shotFiles) sf.append(f);
    o["shot_files"] = sf;
    o["prompt_override"] = promptOverride;
    return o;
}

void Sequence::fromJson(const QJsonObject& o, const QString& shotDir) {
    id = o["id"].toString("Seq 1");
    name = o["name"].toString();
    lipConstraint = o["lip_constraint"].toString();
    cast.clear();
    for (const auto& c : o["cast"].toArray()) cast.append(c.toString());
    scenes.clear();
    for (const auto& c : o["scenes"].toArray()) scenes.append(c.toString());
    props.clear();
    for (const auto& c : o["props"].toArray()) props.append(c.toString());
    promptOverride = o["prompt_override"].toString();
    shotFiles.clear();
    for (const auto& v : o["shot_files"].toArray()) shotFiles.append(v.toString());
    shots.clear();
    if (!shotFiles.isEmpty()) {
        for (const QString& fn : shotFiles) {
            QFile sf(QDir(shotDir).filePath(fn));
            if (!sf.open(QIODevice::ReadOnly)) continue;
            QJsonDocument sd = QJsonDocument::fromJson(sf.readAll());
            sf.close();
            if (sd.isNull()) continue;
            Shot s; s.fromJson(sd.object()); shots.append(s);
        }
    } else {
        for (const auto& s : o["shots"].toArray()) {
            Shot x; x.fromJson(s.toObject()); shots.append(x);
        }
    }
    if (shots.isEmpty()) shots.append(Shot());
}

// ---------------- Settings ----------------
QJsonObject Settings::toJson() const {
    QJsonObject o;
    o["style_constraint"]  = styleConstraint;
    return o;
}

void Settings::fromJson(const QJsonObject& o) {
    styleConstraint = o["style_constraint"].toString();
}

// ---------------- AppConfig ----------------
bool AppConfig::load(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull()) return false;
    QJsonObject o = doc.object();
    commonStyles.clear();
    for (const auto& v : o["common_styles"].toArray()) commonStyles.append(v.toString());
    theme = o["theme"].toString();
    if (theme != "light" && theme != "dark") theme = "dark";
    soundscapes.clear();
    for (const auto& v : o["soundscapes"].toArray())   soundscapes.append(v.toString());
    musics.clear();
    for (const auto& v : o["musics"].toArray())        musics.append(v.toString());
    recentProjects.clear();
    for (const auto& v : o["recent_projects"].toArray()) recentProjects.append(v.toString());
    previewCompare = o["preview_compare"].toBool(false);
    previewAnnot = o["preview_annot"].toBool(false);
    previewAutoRefresh = o["preview_auto_refresh"].toBool(true);
    previewOptCollapsed = o["preview_opt_collapsed"].toBool(false);
    previewOptSplitSizes.clear();
    for (const auto& v : o["preview_opt_split_sizes"].toArray()) previewOptSplitSizes.append(v.toInt());
    previewCompareSplitSizes.clear();
    for (const auto& v : o["preview_compare_split_sizes"].toArray()) previewCompareSplitSizes.append(v.toInt());
    previewStyleSel.clear();
    for (const auto& v : o["preview_style_sel"].toArray()) previewStyleSel.append(v.toInt());
    previewSound = o["preview_sound"].toString();
    previewMusic = o["preview_music"].toString();
    return true;
}

bool AppConfig::save(const QString& path) const {
    QJsonObject o;
    o["common_styles"] = QJsonArray::fromStringList(commonStyles);
    o["soundscapes"]   = QJsonArray::fromStringList(soundscapes);
    o["musics"]        = QJsonArray::fromStringList(musics);
    o["recent_projects"] = QJsonArray::fromStringList(recentProjects);
    o["theme"]         = theme;
    o["preview_compare"] = previewCompare;
    o["preview_annot"] = previewAnnot;
    o["preview_auto_refresh"] = previewAutoRefresh;
    o["preview_opt_collapsed"] = previewOptCollapsed;
    QJsonArray poss;
    for (int x : previewOptSplitSizes) poss.append(x);
    o["preview_opt_split_sizes"] = poss;
    QJsonArray pcs;
    for (int x : previewCompareSplitSizes) pcs.append(x);
    o["preview_compare_split_sizes"] = pcs;
    QJsonArray pss;
    for (int x : previewStyleSel) pss.append(x);
    o["preview_style_sel"] = pss;
    o["preview_sound"] = previewSound;
    o["preview_music"] = previewMusic;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

void AppConfig::ensureDefaults() {
    if (theme != "light" && theme != "dark") theme = "dark";
    if (commonStyles.isEmpty())
        commonStyles << "中式二次元动画风格，新海诚式极致光影，精致写实向背景美术，光线粒子与尘埃浮动，暖色调但对比鲜明。";
    if (soundscapes.isEmpty())
        soundscapes << "夜晚寂静无声，灯光关闭的声音。";
    if (musics.isEmpty())
        musics << "N/A";
}

QString AppConfig::defaultPath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/appconfig.json";
}

// ---------------- Project ----------------
QJsonObject Project::toJson(const QString& base) const {
    QJsonObject o;
    o["version"]       = version;
    o["project_name"]  = projectName;
    o["settings"]      = settings.toJson();
    QJsonArray sa, sc, pp;
    for (const auto& s : subjects) sa.append(s.toJson(base));
    for (const auto& s : scenes)   sc.append(s.toJson(base));
    for (const auto& s : props)    pp.append(s.toJson(base));
    o["subjects"] = sa;
    o["scenes"]   = sc;
    o["props"]    = pp;
    QJsonArray sq;
    for (const auto& s : sequences) sq.append(s.toJson());
    o["sequences"] = sq;
    o["shot_dir"]  = shotsSubdir;
    return o;
}

void Project::fromJson(const QJsonObject& o, const QString& base) {
    version      = o["version"].toInt(1);
    projectName  = o["project_name"].toString("未命名工程");
    settings.fromJson(o["settings"].toObject());
    subjects.clear();
    for (const auto& s : o["subjects"].toArray()) {
        Subject x; x.fromJson(s.toObject(), base); subjects.append(x);
    }
    scenes.clear();
    for (const auto& s : o["scenes"].toArray()) {
        Scene x; x.fromJson(s.toObject(), base); scenes.append(x);
    }
    props.clear();
    for (const auto& s : o["props"].toArray()) {
        Prop x; x.fromJson(s.toObject(), base); props.append(x);
    }
    shotsSubdir = o["shot_dir"].toString();
    QString shotDir = QDir(base).filePath(shotsSubdir);
    sequences.clear();
    for (const auto& s : o["sequences"].toArray()) {
        Sequence x; x.fromJson(s.toObject(), shotDir); sequences.append(x);
    }
}

bool Project::save(const QString& path) {
    QString base = QFileInfo(path).absolutePath();
    QString stem = QFileInfo(path).completeBaseName();
    shotsSubdir = stem + ".shots";
    QString dir = QDir(base).filePath(shotsSubdir);
    QDir().mkpath(dir);

    // 每个镜头单独写文件（跨所有分镜连续编号）
    int counter = 0;
    QSet<QString> used;
    for (auto& seq : sequences) {
        seq.shotFiles.clear();
        for (int j = 0; j < seq.shots.size(); ++j) {
            QString fn = QString("shot_%1.json").arg(++counter, 3, 10, QChar('0'));
            used.insert(fn);
            seq.shotFiles << fn;
            QFile sf(QDir(dir).filePath(fn));
            if (!sf.open(QIODevice::WriteOnly)) return false;
            sf.write(QJsonDocument(seq.shots[j].toJson()).toJson(QJsonDocument::Indented));
            sf.close();
        }
    }
    // 清理已不存在的旧镜头文件
    QDir d(dir);
    for (const QString& fn : d.entryList(QStringList() << "shot_*.json", QDir::Files))
        if (!used.contains(fn)) d.remove(fn);

    QJsonDocument doc(toJson(base));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    baseDir = base;
    filePath = path;
    return true;
}

bool Project::load(const QString& path) {
    QString base = QFileInfo(path).absolutePath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull()) return false;
    fromJson(doc.object(), base);
    baseDir = base;
    filePath = path;
    return true;
}
