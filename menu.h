#ifndef MENU_H
#define MENU_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class menu; }
QT_END_NAMESPACE

class menu : public QWidget
{
    Q_OBJECT

public:
    menu(QWidget *parent = nullptr);
    ~menu();

    void updateInf();
    void switchMiniMode(bool mini);
private:
    Ui::menu *ui;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    bool isDragging = false;
    QPoint dragStartPosition;
};
#endif // MENU_H
