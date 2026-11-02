#pragma once

#include <QString>
#include <QStringList>

struct Project;
struct Sequence;

// 生成时从“应用内配置”中选取的常用信息
struct GenOptions {
    QStringList styles;    // 选中的常用画风（可多条）
    QString soundscape;    // 选中的背景环境声音 overall_soundscape
    QString music;         // 选中的背景音乐 non_diegetic_music
};

// 依据工程数据 + 生成选项，生成 MiniMax H3（六段结构）提示词
QString generatePrompt(const Project& p, const GenOptions& opt);

// 仅生成某一个分镜（Sequence）的提示词（编号按该分镜内统一重新编号）
QString generateSequencePrompt(const Project& p, const Sequence& seq, const GenOptions& opt);

// 带来源标注的对照版本：在由结构化数据生成的提示词行前插入 “// ← 来源：...” 注释行
QString generateAnnotatedSequencePrompt(const Project& p, const Sequence& seq, const GenOptions& opt);
