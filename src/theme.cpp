#include "theme.h"

#include <QApplication>
#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPolygonF>
#include <QStyleFactory>

// ---------------- 深色主题 ----------------
static const char* kDarkQss = R"QSS(
QWidget {
    color: #D7DCE5;
    font-size: 13px;
}

QMainWindow, QDialog {
    background: #1E2228;
}

QMenuBar {
    background: #1A1E24;
    border-bottom: 1px solid #2C333D;
    padding: 3px 6px;
}
QMenuBar::item {
    background: transparent;
    padding: 6px 12px;
    border-radius: 6px;
}
QMenuBar::item:selected { background: #2C3441; }
QMenuBar::item:pressed  { background: #333D4B; }

QMenu {
    background: #232830;
    border: 1px solid #343B47;
    border-radius: 8px;
    padding: 6px;
}
QMenu::item {
    padding: 7px 26px 7px 12px;
    border-radius: 6px;
}
QMenu::item:selected { background: #2E3A52; }
QMenu::item:disabled { color: #6B7687; }
QMenu::separator { height: 1px; background: #343B47; margin: 6px 8px; }

QTreeWidget {
    background: #1B1F25;
    border: 1px solid #2C333D;
    border-radius: 10px;
    padding: 6px;
    outline: none;
}
QTreeWidget::item {
    padding: 6px 4px;
    border-radius: 6px;
    color: #C9D2E0;
}
QTreeWidget::item:hover { background: #262D38; }
QTreeWidget::item:selected {
    background: #2D3B58;
    color: #E8EEFF;
}
QTreeView::branch { background: transparent; }

QTableWidget {
    background: #1B1F25;
    border: 1px solid #2C333D;
    border-radius: 10px;
    gridline-color: #2C333D;
    padding: 4px;
    outline: none;
}
QTableWidget::item {
    padding: 5px 6px;
    border-radius: 4px;
    color: #C9D2E0;
}
QTableWidget::item:selected {
    background: #2D3B58;
    color: #E8EEFF;
}

QHeaderView::section {
    background: #232830;
    color: #9FB0C9;
    border: none;
    padding: 7px 8px;
    font-weight: 600;
}

QScrollArea {
    background: #1B1F25;
    border: none;
}
QScrollArea > QWidget > QWidget { background: transparent; }

QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: #161A20;
    border: 1px solid #343B47;
    border-radius: 7px;
    padding: 6px 9px;
    color: #DCE3EF;
    selection-background-color: #3A5AA8;
    selection-color: #FFFFFF;
}
QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover, QComboBox:hover {
    border-color: #4A5666;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus,
QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid #5B8CFF;
}
QLineEdit:disabled, QPlainTextEdit:disabled, QComboBox:disabled {
    color: #6B7687;
    background: #1B1F25;
}
QPlainTextEdit, QTextEdit { padding: 9px; }

QComboBox::drop-down {
    border: none;
    width: 22px;
}
QComboBox::down-arrow {
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #8B95A7;
    margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: #232830;
    border: 1px solid #343B47;
    border-radius: 8px;
    padding: 4px;
    selection-background-color: #2E3A52;
    selection-color: #E8EEFF;
    outline: none;
}

QPushButton {
    background: #2A323E;
    border: 1px solid #3A4453;
    border-radius: 7px;
    padding: 7px 15px;
    color: #DCE3EF;
}
QPushButton:hover  { background: #333D4B; border-color: #4A5666; }
QPushButton:pressed { background: #242B36; }
QPushButton:disabled { color: #6B7687; background: #232830; border-color: #2C333D; }
QPushButton:default { border-color: #5B8CFF; }

QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #4A5666;
    border-radius: 4px;
    background: #161A20;
}
QCheckBox::indicator:hover { border-color: #5B8CFF; }
QCheckBox::indicator:checked {
    background: #5B8CFF;
    border-color: #5B8CFF;
}

QGroupBox {
    background: #20252C;
    border: 1px solid #2C333D;
    border-radius: 10px;
    margin-top: 16px;
    padding: 14px 12px 10px 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: #9FB0C9;
    font-weight: 600;
}

QLabel { background: transparent; color: #C9D2E0; }

QSplitter::handle { background: #2C333D; }
QSplitter::handle:horizontal { width: 2px; }
QSplitter::handle:vertical { height: 2px; }

QStatusBar {
    background: #1A1E24;
    border-top: 1px solid #2C333D;
    color: #8B95A7;
}
QStatusBar::item { border: none; }

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #3A4453;
    border-radius: 5px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #4A5666; }
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: #3A4453;
    border-radius: 5px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #4A5666; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QToolTip {
    background: #2A323E;
    color: #E6ECFF;
    border: 1px solid #3A4453;
    padding: 5px 8px;
    border-radius: 6px;
}

QTabWidget::pane {
    border: 1px solid #2C333D;
    border-radius: 8px;
    top: -1px;
}
QTabBar::tab {
    background: #232830;
    color: #9FB0C9;
    padding: 7px 14px;
    border: 1px solid #2C333D;
    border-bottom: none;
    border-top-left-radius: 7px;
    border-top-right-radius: 7px;
    margin-right: 2px;
}
QTabBar::tab:selected { background: #2D3B58; color: #E8EEFF; }
)QSS";

// ---------------- 浅色主题 ----------------
static const char* kLightQss = R"QSS(
QWidget {
    color: #2B3242;
    font-size: 13px;
}

QMainWindow, QDialog {
    background: #F4F6FA;
}

QMenuBar {
    background: #FFFFFF;
    border-bottom: 1px solid #E2E6EE;
    padding: 3px 6px;
}
QMenuBar::item {
    background: transparent;
    padding: 6px 12px;
    border-radius: 6px;
}
QMenuBar::item:selected { background: #E6ECF7; }
QMenuBar::item:pressed  { background: #DDE6F4; }

QMenu {
    background: #FFFFFF;
    border: 1px solid #DDE3ED;
    border-radius: 8px;
    padding: 6px;
}
QMenu::item {
    padding: 7px 26px 7px 12px;
    border-radius: 6px;
}
QMenu::item:selected { background: #E6ECF7; color: #1B3A8C; }
QMenu::item:disabled { color: #A7AEBB; }
QMenu::separator { height: 1px; background: #E2E6EE; margin: 6px 8px; }

QTreeWidget {
    background: #FFFFFF;
    border: 1px solid #E2E6EE;
    border-radius: 10px;
    padding: 6px;
    outline: none;
}
QTreeWidget::item {
    padding: 6px 4px;
    border-radius: 6px;
    color: #3A4252;
}
QTreeWidget::item:hover { background: #EEF2F9; }
QTreeWidget::item:selected {
    background: #D7E3FF;
    color: #15327A;
}
QTreeView::branch { background: transparent; }

QTableWidget {
    background: #FFFFFF;
    border: 1px solid #E2E6EE;
    border-radius: 10px;
    gridline-color: #E2E6EE;
    padding: 4px;
    outline: none;
}
QTableWidget::item {
    padding: 5px 6px;
    border-radius: 4px;
    color: #3A4252;
}
QTableWidget::item:selected {
    background: #D7E3FF;
    color: #15327A;
}

QHeaderView::section {
    background: #F4F6FA;
    color: #5A6478;
    border: none;
    padding: 7px 8px;
    font-weight: 600;
}

QScrollArea {
    background: #FFFFFF;
    border: none;
}
QScrollArea > QWidget > QWidget { background: transparent; }

QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: #FFFFFF;
    border: 1px solid #CBD2E0;
    border-radius: 7px;
    padding: 6px 9px;
    color: #2B3242;
    selection-background-color: #3B6CF6;
    selection-color: #FFFFFF;
}
QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover, QComboBox:hover {
    border-color: #B6C0D2;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus,
QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid #3B6CF6;
}
QLineEdit:disabled, QPlainTextEdit:disabled, QComboBox:disabled {
    color: #A7AEBB;
    background: #F4F6FA;
}
QPlainTextEdit, QTextEdit { padding: 9px; }

QComboBox::drop-down {
    border: none;
    width: 22px;
}
QComboBox::down-arrow {
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #6B7687;
    margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: #FFFFFF;
    border: 1px solid #DDE3ED;
    border-radius: 8px;
    padding: 4px;
    selection-background-color: #E6ECF7;
    selection-color: #1B3A8C;
    outline: none;
}
QComboBox QAbstractItemView::item { padding: 6px 10px; }

QPushButton {
    background: #EFF2F8;
    border: 1px solid #CBD2E0;
    border-radius: 7px;
    padding: 7px 15px;
    color: #2B3242;
}
QPushButton:hover  { background: #E2E8F4; border-color: #B6C0D2; }
QPushButton:pressed { background: #D7E0F0; }
QPushButton:disabled { color: #A7AEBB; background: #F4F6FA; border-color: #E2E6EE; }
QPushButton:default { border-color: #3B6CF6; }

QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #B6C0D2;
    border-radius: 4px;
    background: #FFFFFF;
}
QCheckBox::indicator:hover { border-color: #3B6CF6; }
QCheckBox::indicator:checked {
    background: #3B6CF6;
    border-color: #3B6CF6;
}

QGroupBox {
    background: #FFFFFF;
    border: 1px solid #E2E6EE;
    border-radius: 10px;
    margin-top: 16px;
    padding: 14px 12px 10px 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: #5A6478;
    font-weight: 600;
}

QLabel { background: transparent; color: #3A4252; }

QSplitter::handle { background: #E2E6EE; }
QSplitter::handle:horizontal { width: 2px; }
QSplitter::handle:vertical { height: 2px; }

QStatusBar {
    background: #FFFFFF;
    border-top: 1px solid #E2E6EE;
    color: #6B7687;
}
QStatusBar::item { border: none; }

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #C7CEDB;
    border-radius: 5px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #AEB8C9; }
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: #C7CEDB;
    border-radius: 5px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #AEB8C9; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QToolTip {
    background: #FFFFFF;
    color: #2B3242;
    border: 1px solid #C7CEDB;
    padding: 5px 8px;
    border-radius: 6px;
}

QTabWidget::pane {
    border: 1px solid #E2E6EE;
    border-radius: 8px;
    top: -1px;
}
QTabBar::tab {
    background: #F4F6FA;
    color: #5A6478;
    padding: 7px 14px;
    border: 1px solid #E2E6EE;
    border-bottom: none;
    border-top-left-radius: 7px;
    border-top-right-radius: 7px;
    margin-right: 2px;
}
QTabBar::tab:selected { background: #D7E3FF; color: #15327A; }
)QSS";

static void setDarkPalette(QApplication* app) {
    QPalette pal;
    pal.setColor(QPalette::Window, QColor("#1E2228"));
    pal.setColor(QPalette::WindowText, QColor("#D7DCE5"));
    pal.setColor(QPalette::Base, QColor("#161A20"));
    pal.setColor(QPalette::AlternateBase, QColor("#1B1F25"));
    pal.setColor(QPalette::ToolTipBase, QColor("#2A323E"));
    pal.setColor(QPalette::ToolTipText, QColor("#E6ECFF"));
    pal.setColor(QPalette::Text, QColor("#DCE3EF"));
    pal.setColor(QPalette::Button, QColor("#2A323E"));
    pal.setColor(QPalette::ButtonText, QColor("#DCE3EF"));
    pal.setColor(QPalette::BrightText, QColor("#FF6B6B"));
    pal.setColor(QPalette::Link, QColor("#5B8CFF"));
    pal.setColor(QPalette::Highlight, QColor("#2D3B58"));
    pal.setColor(QPalette::HighlightedText, QColor("#E8EEFF"));
    pal.setColor(QPalette::PlaceholderText, QColor("#6B7687"));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#6B7687"));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6B7687"));
    app->setPalette(pal);
}

static void setLightPalette(QApplication* app) {
    QPalette pal;
    pal.setColor(QPalette::Window, QColor("#F4F6FA"));
    pal.setColor(QPalette::WindowText, QColor("#2B3242"));
    pal.setColor(QPalette::Base, QColor("#FFFFFF"));
    pal.setColor(QPalette::AlternateBase, QColor("#F4F6FA"));
    pal.setColor(QPalette::ToolTipBase, QColor("#FFFFFF"));
    pal.setColor(QPalette::ToolTipText, QColor("#2B3242"));
    pal.setColor(QPalette::Text, QColor("#2B3242"));
    pal.setColor(QPalette::Button, QColor("#EFF2F8"));
    pal.setColor(QPalette::ButtonText, QColor("#2B3242"));
    pal.setColor(QPalette::BrightText, QColor("#D23B3B"));
    pal.setColor(QPalette::Link, QColor("#3B6CF6"));
    pal.setColor(QPalette::Highlight, QColor("#D7E3FF"));
    pal.setColor(QPalette::HighlightedText, QColor("#15327A"));
    pal.setColor(QPalette::PlaceholderText, QColor("#9AA3B2"));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#9AA3B2"));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9AA3B2"));
    app->setPalette(pal);
}

void Theme::apply(QApplication* app, const QString& style) {
    app->setStyle(QStyleFactory::create("Fusion"));

    if (style == "light") {
        setLightPalette(app);
        app->setStyleSheet(QString::fromUtf8(kLightQss));
    } else {
        setDarkPalette(app);
        app->setStyleSheet(QString::fromUtf8(kDarkQss));
    }

    QFont f = app->font();
    f.setFamily("Microsoft YaHei UI");
    f.setPointSize(9);
    app->setFont(f);
}

QIcon Theme::icon(int kind) {
    static QHash<int, QIcon> cache;
    if (cache.contains(kind)) return cache.value(kind);

    const int S = 64;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    switch (kind) {
    case Project: {
        p.setBrush(QColor("#93A6C4"));
        p.drawRoundedRect(QRectF(16, 10, 32, 44), 6, 6);
        p.setBrush(QColor("#5C6E88"));
        QPolygonF fold;
        fold << QPointF(36, 10) << QPointF(48, 22) << QPointF(36, 22);
        p.drawPolygon(fold);
        p.setPen(QPen(QColor("#2A323E"), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(23, 31), QPointF(41, 31));
        p.drawLine(QPointF(23, 39), QPointF(41, 39));
        break;
    }
    case Folder: {
        p.setBrush(QColor("#5B8CFF"));
        p.drawRoundedRect(QRectF(10, 18, 44, 32), 6, 6);
        p.setBrush(QColor("#7BA6FF"));
        p.drawRoundedRect(QRectF(10, 11, 24, 15), 5, 5);
        break;
    }
    case Character: {
        p.setBrush(QColor("#6E9BFF"));
        p.drawEllipse(QPointF(32, 23), 10.5, 10.5);
        p.drawRoundedRect(QRectF(15, 36, 34, 21), 10.5, 10.5);
        break;
    }
    case Scene: {
        p.setBrush(QColor("#4FC08D"));
        p.drawEllipse(QPointF(44, 20), 7, 7);
        QPainterPath mtn;
        mtn.moveTo(8, 50);
        mtn.lineTo(26, 23);
        mtn.lineTo(40, 43);
        mtn.lineTo(48, 33);
        mtn.lineTo(57, 50);
        mtn.closeSubpath();
        p.drawPath(mtn);
        break;
    }
    case Prop: {
        p.setBrush(QColor("#F0A24B"));
        p.drawRoundedRect(QRectF(12, 25, 40, 27), 5, 5);
        p.setBrush(QColor("#FFC078"));
        QPolygonF top;
        top << QPointF(12, 25) << QPointF(22, 14) << QPointF(52, 14) << QPointF(42, 25);
        p.drawPolygon(top);
        break;
    }
    case Sequence: {
        p.setBrush(QColor("#B07CFF"));
        p.drawRoundedRect(QRectF(10, 17, 44, 30), 5, 5);
        p.setBrush(QColor("#1B1F25"));
        for (int i = 0; i < 4; ++i) {
            p.drawRoundedRect(QRectF(13 + i * 11, 21, 5, 5), 1.5, 1.5);
            p.drawRoundedRect(QRectF(13 + i * 11, 38, 5, 5), 1.5, 1.5);
        }
        p.setBrush(QColor("#E9DCFF"));
        p.drawRoundedRect(QRectF(15, 29, 34, 6), 3, 3);
        break;
    }
    case Shot: {
        p.setBrush(QColor("#58C4DD"));
        p.drawRoundedRect(QRectF(8, 20, 34, 26), 5, 5);
        QPolygonF lens;
        lens << QPointF(42, 28) << QPointF(56, 20) << QPointF(56, 46) << QPointF(42, 38);
        p.drawPolygon(lens);
        p.setBrush(QColor("#1B1F25"));
        p.drawEllipse(QPointF(24, 33), 6.5, 6.5);
        break;
    }
    default:
        break;
    }

    p.end();
    QIcon ic(pm);
    cache.insert(kind, ic);
    return ic;
}
