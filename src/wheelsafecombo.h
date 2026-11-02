#pragma once

#include <QComboBox>
#include <QWheelEvent>

// 鼠标滚轮不改变下拉选项：忽略滚轮事件并让其向上冒泡到外层滚动区域，
// 避免鼠标划过或悬停在下拉框上时误改选中项。
class WheelSafeComboBox : public QComboBox {
public:
    using QComboBox::QComboBox;
protected:
    void wheelEvent(QWheelEvent* e) override { e->ignore(); }
};
