#pragma once

#include <QIcon>

class QApplication;

// 全局界面主题：QSS 样式 + 程序内绘制的树节点图标（不依赖外部图片资源）
namespace Theme {

enum IconKind {
    Project = 0,   // 工程根节点
    Folder,        // 资源 / 分组
    Character,     // 角色
    Scene,         // 场景
    Prop,          // 道具
    Sequence,      // 分镜
    Shot           // 镜头
};

void apply(QApplication* app, const QString& style = "dark");
QIcon icon(int kind);

} // namespace Theme
