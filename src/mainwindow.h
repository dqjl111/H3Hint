#pragma once

#include <QMainWindow>
#include <functional>
#include "project.h"

class QTreeWidget;
class QTreeWidgetItem;
class QScrollArea;
class QMenu;
class PreviewDock;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onGenerate();
    void onLoadExample();
    void onAbout();
    void onAppSettings();
    void onTreeCurrentChanged(QTreeWidgetItem* cur, QTreeWidgetItem* prev);
    void markDirty();

private:
    void buildTree();
    void buildEditor(QTreeWidgetItem* item);
    void setEditor(QWidget* w);
    void selectItem(const QString& kind, int idx);
    void updateCurrentLabel();
    void openProjectFile(const QString& f);
    void openRecent(const QString& f);
    void addRecentFile(const QString& f);
    void refreshRecentMenu();
    QWidget* makeRow(const QString& label, QWidget* field);
    QWidget* makePathRow(const QString& label, std::function<QString()> get,
                         std::function<void(const QString&)> set, const QString& dlgTitle);

    // 编辑器构造
    QWidget* buildSettingsEditor();
    QWidget* buildSubjectsHeader();
    QWidget* buildSubjectEditor(int idx);
    QWidget* buildScenesHeader();
    QWidget* buildSceneEditor(int idx);
    QWidget* buildPropsHeader();
    QWidget* buildPropEditor(int idx);
    QWidget* buildSequencesHeader();
    QWidget* buildSequenceEditor(int seqIdx);
    QWidget* buildShotEditor(int seqIdx, int shotIdx);

    void fillAudios(QWidget* container, int sIdx);
    void fillCast(QWidget* container, int seqIdx);
    void fillScenes(QWidget* container, int seqIdx);
    void fillProps(QWidget* container, int seqIdx);
    void fillSubShots(QWidget* container, int seqIdx, int shotIdx);
    void fillSpeeches(QWidget* container, int seqIdx, int shotIdx, int subIdx);
    void selectShotNode(int seqIdx, int shotIdx);

    // 资源导入 / 导出（kind: -1 全部，0 角色，1 场景，2 道具）
    void doImportAssets();
    void doExportAssets(int kind);

    Project proj;
    AppConfig appCfg;       // 应用内配置（独立于工程文件）
    QString appCfgPath;     // 应用内配置文件路径
    QString curFile;
    bool dirty = false;
    bool seqPreviewMode = false;  // 分镜编辑页上次停留：false=分镜编辑(第1页)，true=提示词预览(第2页)
    QTreeWidget* tree = nullptr;
    QScrollArea* scroll = nullptr;
    QMenu* recentMenu = nullptr;
};
