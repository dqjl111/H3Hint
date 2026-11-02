#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>

// 角色音色音频参考
struct AudioRef {
    QString id = "Audio 1";      // 内部编号（生成用，界面不显示、不可编辑）
    QString name;                // 音频名称（界面显示；空则可选择文件时用文件名填充）
    QString path;
    QString desc = "提供音色、语速与发声习惯参考，仅作音色参考，不复制音频内容。";
    QJsonObject toJson(const QString& base) const;
    void fromJson(const QJsonObject& o, const QString& base);
};

// 角色（普通角色 或 Q 版内心小人）
struct Subject {
    QString id = "Subject 1";
    QString name;
    QString alias;                // 备注名：仅用于界面显示，区分同名角色，不参与生成
    QString desc;
    bool isQ = false;
    QString qId;                 // 关联的 Q 版小人 id（普通角色填写，可选）
    QString viewImage;           // 单张四视图参考图（面部特写/全身正面/侧面/背面 同图）
    QList<AudioRef> audios;      // 音色音频参考
    QJsonObject toJson(const QString& base) const;
    void fromJson(const QJsonObject& o, const QString& base);
};

// 场景图片资源
struct Scene {
    QString id = "Scene 1";
    QString label;
    QString path;
    QString desc;
    QJsonObject toJson(const QString& base) const;
    void fromJson(const QJsonObject& o, const QString& base);
};

// 道具图片资源
struct Prop {
    QString id = "Prop 1";
    QString label;
    QString path;
    QString desc;
    QJsonObject toJson(const QString& base) const;
    void fromJson(const QJsonObject& o, const QString& base);
};

// 一段台词：<Subject N>(S<n>)[备注]使用<Audio N>说：“中文内容”（S<n> 为整段提示词内跨镜头连续编号）
struct Speech {
    QString subject;
    bool s2 = false;
    QString note;     // 备注（说话话式），没有则空
    QString audio;
    QString lead;     // 说话前修饰，如“担心的”
    QString text;     // 说话内容（不含引号）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
};

// 分镜内一个段落
struct SubShot {
    QString caption;
    QList<Speech> speeches;
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
};

// 分镜下的一个镜头（子段）
struct Shot {
    QString id = "Shot 1";          // 内部编号（仅文件命名，界面不显示）
    QString label;                  // 显示名（可选），无则“镜头N”
    QList<SubShot> subShots;
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
};

// 分镜：视频级容器，包含多个镜头(Shot)；编号在分镜内统一，与项目编号无关
struct Sequence {
    QString id = "Seq 1";           // 内部编号（界面不显示）
    QString name;                   // 显示名（可选），无则“分镜N”
    QString lipConstraint;          // 嘴型约束（本分镜）
    QList<QString> cast;            // 出场角色 id 列表（顺序即 <Subject N> 编号）
    QList<QString> scenes;          // 本分镜使用的场景 id 列表（生成时并入 <Subject N>）
    QList<QString> props;           // 本分镜使用的道具 id 列表（生成时并入 <Subject N>）
    QList<Shot> shots;              // 本分镜下的多个镜头
    QStringList shotFiles;          // 镜头文件名列表（相对 shotsSubdir，由 save 填充，不序列化到主文件以外）
    QString promptOverride;         // 手动编辑后的提示词全文（非空时覆盖自动生成；预览/导出优先使用）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o, const QString& shotDir);
};

// 工程级约束（画风约束属于具体工程；嘴型约束已下放到分镜）
struct Settings {
    QString styleConstraint;        // 画风约束（全局）
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
};

// 应用内配置：独立于工程文件，保存可复用的常用画风 / 环境声音 / 背景音乐
struct AppConfig {
    QStringList commonStyles;       // 常用画风，可多条
    QStringList soundscapes;        // 背景环境声音 overall_soundscape，可多条
    QStringList musics;             // 背景音乐 non_diegetic_music，可多条（含 N/A）
    QStringList recentProjects;     // 最近打开的工程文件路径
    QString theme = "dark";         // 界面主题："dark" 深色 / "light" 浅色

    // 预览面板布局记忆（跨分镜切换保持一致）
    bool previewCompare = false;        // 对照模式（右侧显示结构化源数据）
    bool previewAnnot = false;          // 显示来源标注（编辑区内逐行注释）
    bool previewAutoRefresh = true;     // 自动刷新
    bool previewOptCollapsed = false;   // “生成选项”分组折叠隐藏
    QList<int> previewOptSplitSizes;    // “生成选项”竖向分隔条的各栏尺寸（记忆拖拽后的大小）
    QList<int> previewCompareSplitSizes; // 对照区（提示词/表格）竖向分隔条尺寸
    QList<int> previewStyleSel;         // 选中的常用画风索引（空=全部选中）
    QString previewSound;               // 背景环境声音当前选择
    QString previewMusic;               // 背景音乐当前选择

    bool load(const QString& path);
    bool save(const QString& path) const;
    void ensureDefaults();
    static QString defaultPath();
};

struct Project {
    int version = 1;
    QString projectName = "未命名工程";
    Settings settings;
    QList<Subject> subjects;
    QList<Scene> scenes;
    QList<Prop> props;
    QList<Sequence> sequences;
    QString baseDir;   // 工程文件所在目录（不序列化）
    QString filePath;  // 工程文件路径（不序列化）
    QString shotsSubdir;   // 镜头文件子目录名（相对工程目录）

    QJsonObject toJson(const QString& base) const;
    void fromJson(const QJsonObject& o, const QString& base);
    bool save(const QString& path);
    bool load(const QString& path);

    static QString toRel(const QString& p, const QString& base);
    static QString toAbs(const QString& p, const QString& base);
};
