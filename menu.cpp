#include "menu.h"
#include "ui_menu.h"
#include <nvml.h>
#include <qtimer.h>
#include<windows.h>
#include <QMouseEvent>
#include <qmenu.h>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QFont>


bool mini = false; // mini模式

QString getGpuName(int index)
{
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(index, &device);
    if (result != NVML_SUCCESS) {
        return "Unknown GPU";
    }

    char name[64];
    result = nvmlDeviceGetName(device, name, sizeof(name));
    if (result != NVML_SUCCESS) {
        return "Unknown GPU";
    }

    return QString::fromLatin1(name);
}

int getGpuTemp(int index)
{
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(index, &device);
    if (result != NVML_SUCCESS) {
        return -1;
    }

    unsigned int temperature = 0;
    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    if (result != NVML_SUCCESS) {
        return -1;
    }

    return (int)temperature;
}

QString getCpuName()
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0,
        KEY_READ,
        &hKey
        );

    if (result != ERROR_SUCCESS)
        return "Unknown CPU";

    char cpuName[256];
    DWORD bufSize = sizeof(cpuName);
    result = RegQueryValueExA(
        hKey,
        "ProcessorNameString",
        NULL,
        NULL,
        (LPBYTE)cpuName,
        &bufSize
        );

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS)
        return "Unknown CPU";

    return QString::fromLatin1(cpuName);
}

bool initNvml()
{
    nvmlReturn_t result = nvmlInit();
    return result == NVML_SUCCESS;
}

void shutdownNvml()
{
    nvmlShutdown();
}

double getCpuUsage()
{
    static FILETIME prevIdleTime = {0, 0};
    static FILETIME prevKernelTime = {0, 0};
    static FILETIME prevUserTime = {0, 0};

    FILETIME idleTime, kernelTime, userTime;

    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return 0.0;

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart   = idleTime.dwLowDateTime;
    idle.HighPart  = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart= kernelTime.dwHighDateTime;
    user.LowPart   = userTime.dwLowDateTime;
    user.HighPart  = userTime.dwHighDateTime;

    // 时间差
    ULONGLONG idleDiff   = idle.QuadPart - ((ULARGE_INTEGER&)prevIdleTime).QuadPart;
    ULONGLONG kernelDiff = kernel.QuadPart - ((ULARGE_INTEGER&)prevKernelTime).QuadPart;
    ULONGLONG userDiff   = user.QuadPart - ((ULARGE_INTEGER&)prevUserTime).QuadPart;

    // 保存当前值
    prevIdleTime = idleTime;
    prevKernelTime = kernelTime;
    prevUserTime = userTime;

    ULONGLONG total = kernelDiff + userDiff;
    if (total == 0)
        return 0.0;

    double usage = (1.0 - (double)idleDiff / total) * 100.0;

    return usage;   // 0 ~ 100
}

qulonglong getCpuTotalRam()
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return mem.ullTotalPhys;  // bytes
}

qulonglong getCpuCurRam()
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return mem.ullTotalPhys - mem.ullAvailPhys;  // used = total - available
}

qulonglong getGpuRam(int index)
{
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(index, &device);
    if (result != NVML_SUCCESS) return 0;

    nvmlMemory_t memInfo;
    result = nvmlDeviceGetMemoryInfo(device, &memInfo);
    if (result != NVML_SUCCESS) return 0;

    return memInfo.used;    // bytes
}

qulonglong getGpuTotalRam(int index)
{
    nvmlDevice_t device;
    nvmlReturn_t result = nvmlDeviceGetHandleByIndex(index, &device);
    if (result != NVML_SUCCESS) {
        return 0;   // 失败返回 0
    }

    nvmlMemory_t memInfo;
    result = nvmlDeviceGetMemoryInfo(device, &memInfo);
    if (result != NVML_SUCCESS) {
        return 0;
    }

    return static_cast<qulonglong>(memInfo.total);    // bytes
}

int getGpuUsage(int index)
{
    nvmlDevice_t device;
    nvmlReturn_t result;

    result = nvmlDeviceGetHandleByIndex(index, &device);
    if (result != NVML_SUCCESS) {
        return -1; // 错误
    }

    nvmlUtilization_t utilization;
    result = nvmlDeviceGetUtilizationRates(device, &utilization);
    if (result != NVML_SUCCESS) {
        return -1;
    }

    return utilization.gpu;   // GPU 核心使用率（0~100）
}




QString percentToColor(int percent)
{
    const int low = 10;
    const int high = 60;
    percent = qBound(0, percent, 100);
    int r, g, b;
    const QColor L(4, 209, 22);
    const QColor M(255, 229, 6);
    const QColor H(255, 0, 0);

    if (percent < low) {
        r = L.red();
        g = L.green();
        b = L.blue();
    } else if (percent < high) {
        double t = static_cast<double>(percent - low)
                   / (high - low);

        r = qRound(L.red()   + t * (M.red()   - L.red()));
        g = qRound(L.green() + t * (M.green() - L.green()));
        b = qRound(L.blue()  + t * (M.blue()  - L.blue()));
    } else {
        double t = static_cast<double>(percent - high)
                   / (100 - high);
        r = qRound(M.red()   + t * (H.red()   - M.red()));
        g = qRound(M.green() + t * (H.green() - M.green()));
        b = qRound(M.blue()  + t * (H.blue()  - M.blue()));
    }

    return QString("rgb(%1,%2,%3)").arg(r).arg(g).arg(b);
}

void menu::updateInf()
{
    // ----------- 数据获取 -----------
    double cpuUsage = getCpuUsage();
    int gpuUsage = getGpuUsage(0);

    qulonglong totalRam = getCpuTotalRam();
    qulonglong usedRam  = getCpuCurRam();

    qulonglong gpuRamUsed  = getGpuRam(0);
    qulonglong gpuRamTotal = getGpuTotalRam(0);

    auto toGB = [](qulonglong bytes){ return bytes / (1024.0 * 1024 * 1024); };
    double totalRamGB = toGB(totalRam);
    double usedRamGB = toGB(usedRam);
    double gpuRamTotalGB = toGB(gpuRamTotal);
    double gpuRamGB  = toGB(gpuRamUsed);

    int ramPercent     = usedRam * 100 / totalRam;
    int gpuRamPercent  = gpuRamUsed * 100 / gpuRamTotal;

    // ----------- 文本更新 -----------
    ui->cpuVal->setText(QString::number(cpuUsage,  'f', 1) + "%");
    ui->gpuVal->setText(QString::number(gpuUsage,  'f', 0) + "%");
    ui->cpuramVal->setText(QString::number(usedRamGB, 'f', 1) + "/" +
                           QString::number(totalRamGB, 'f', 1) + "G");
    ui->gpuramVal->setText(QString::number(gpuRamGB, 'f', 1) + "/" +
                           QString::number(gpuRamTotalGB, 'f', 1) + "G");

    // ----------- 进度条 QSS -----------
    auto makeBarQss = [&](int percent, int radius = 3) {
        QString color = percentToColor(percent);

        return QString(
                   "QProgressBar {"
                   "    border: 1px solid #333;"
                   "    border-radius: %1px;"
                   "    background: #FFFFFF;"
                   "    text-align: center;"
                   "    color: white;"
                   "    height: 12px;"
                   "}"
                   "QProgressBar::chunk {"
                   "    background-color: %2;"
                   "    border-radius: %1px;"
                   "}").arg(radius).arg(color);
    };

    int radius = mini ? 0 : 6;
    ui->cbar->setStyleSheet(makeBarQss(qRound(cpuUsage), radius));
    ui->crbar->setStyleSheet(makeBarQss(ramPercent, radius));
    ui->gbar->setStyleSheet(makeBarQss(gpuUsage, radius));
    ui->grbar->setStyleSheet(makeBarQss(gpuRamPercent, radius));

    // ----------- 更新进度值 -----------
    ui->cbar->setValue(qRound(cpuUsage));
    ui->crbar->setValue(ramPercent);
    ui->gbar->setValue(gpuUsage);
    ui->grbar->setValue(gpuRamPercent);
}

void menu::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    if (windowFlags() & Qt::WindowStaysOnTopHint) {
        menu.addAction("取消置顶", [this]() {
            setWindowFlag(Qt::WindowStaysOnTopHint, false);
            show();
        });
    } else {
        menu.addAction("置顶显示", [this]() {
            setWindowFlag(Qt::WindowStaysOnTopHint, true);
            show();
        });
    }

    menu.addSeparator();

    menu.addAction(mini ? "切换到正常模式" : "切换到迷你模式", [=]() mutable{
        mini = !mini;
        switchMiniMode(mini);
    });

    menu.addSeparator();
    menu.addAction("退出", qApp, &QApplication::quit);

    menu.exec(event->globalPos());
}




void menu::switchMiniMode(bool mini)
{
    QParallelAnimationGroup *group = new QParallelAnimationGroup(this);

    // ---------- 窗口 geometry ----------
    QRect winStart = this->geometry();
    QRect winEnd = mini ? QRect(winStart.topLeft(), QSize(180, 100))
                        : QRect(winStart.topLeft(), QSize(431, 251));
    QPropertyAnimation *winAnim = new QPropertyAnimation(this, "geometry");
    winAnim->setDuration(1200);
    winAnim->setStartValue(winStart);
    winAnim->setEndValue(winEnd);
    winAnim->setEasingCurve(QEasingCurve::InOutBack);
    group->addAnimation(winAnim);

    // ---------- 背景 label 缩放 ----------
    QRect labelStart = ui->label->geometry();
    QRect labelEnd = mini ? QRect(0,0,180,100) : QRect(0,0,431,251);
    QPropertyAnimation *labelAnim = new QPropertyAnimation(ui->label, "geometry");
    labelAnim->setDuration(1200);
    labelAnim->setStartValue(labelStart);
    labelAnim->setEndValue(labelEnd);
    labelAnim->setEasingCurve(QEasingCurve::InOutBack);
    group->addAnimation(labelAnim);

    // ---------- 背景 label_2~5 淡出 ----------
    auto fadeWidget = [&](QWidget* w, bool show){
        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(w);
        w->setGraphicsEffect(eff);
        QPropertyAnimation *anim = new QPropertyAnimation(eff, "opacity");
        anim->setDuration(1200);
        anim->setStartValue(show ? 0.0 : 1.0);
        anim->setEndValue(show ? 1.0 : 0.0);
        anim->setEasingCurve(QEasingCurve::InOutQuad);
        group->addAnimation(anim);
    };

    fadeWidget(ui->label_2, !mini);
    fadeWidget(ui->label_3, !mini);
    fadeWidget(ui->label_4, !mini);
    fadeWidget(ui->label_5, !mini);
    fadeWidget(ui->cpuVal, !mini);
    fadeWidget(ui->cpuramVal, !mini);
    fadeWidget(ui->gpuVal, !mini);
    fadeWidget(ui->gpuramVal, !mini);


    auto animLine = [&](QLabel* label, QProgressBar* bar,
                        QRect miniLabelRect, QRect normalLabelRect,
                        QRect miniBarRect, QRect normalBarRect,
                        int miniFont, int normalFont,int dur=100)
    {
        QPropertyAnimation *labelAnim = new QPropertyAnimation(label, "geometry");
        labelAnim->setDuration(900);
        labelAnim->setStartValue(label->geometry());
        labelAnim->setEndValue(mini ? miniLabelRect : normalLabelRect);
        labelAnim->setEasingCurve(QEasingCurve::InOutBack);

        auto fontAnim = new QVariantAnimation(this);
        fontAnim->setDuration(800);
        fontAnim->setStartValue(mini ? normalFont : miniFont);
        fontAnim->setEndValue(mini ? miniFont : normalFont);
        fontAnim->setEasingCurve(QEasingCurve::InOutBack);

        connect(fontAnim, &QVariantAnimation::valueChanged, [label](const QVariant &value){
            QFont f = label->font();
            f.setPointSize(value.toInt());
            f.setBold(true);
            label->setFont(f);
        });

        QPropertyAnimation *barAnim = new QPropertyAnimation(bar, "geometry");
        barAnim->setDuration(900);
        barAnim->setStartValue(bar->geometry());
        barAnim->setEndValue(mini ? miniBarRect : normalBarRect);
        barAnim->setEasingCurve(QEasingCurve::InOutBack);


        QParallelAnimationGroup *gp = new QParallelAnimationGroup(this);
        gp->addAnimation(labelAnim);
        gp->addAnimation(fontAnim);
        gp->addAnimation(barAnim);
        QTimer::singleShot(dur,[=](){
            gp->start(QAbstractAnimation::DeleteWhenStopped);
        });
    };



    animLine(ui->label1, ui->cbar,
             QRect(10,10,40,16), QRect(30,40,101,31),
             QRect(50,10,115,16), QRect(30,80,171,16),
             10,24,0);

    animLine(ui->label2, ui->crbar,
             QRect(10,30,40,16), QRect(230,40,81,31),
             QRect(50,30,115,16), QRect(230,80,171,16),
             10,24,100);

    animLine(ui->label3, ui->gbar,
             QRect(10,50,40,16), QRect(30,130,91,31),
             QRect(50,50,115,16), QRect(30,170,171,16),
             10,24,200);

    animLine(ui->label4, ui->grbar,
             QRect(10,70,40,16), QRect(230,130,81,31),
             QRect(50,70,115,16), QRect(230,170,171,16),
             10,24,300);

    group->start(QAbstractAnimation::DeleteWhenStopped);
    updateInf();
}


void menu::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        dragStartPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void menu::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - dragStartPosition);
        event->accept();
    }
}

void menu::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    }
}

menu::menu(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::menu)
{
    ui->setupUi(this);


    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    if (!initNvml() ) {
        qDebug() << "NVML init failed!";
        return;
    }


    ui->cpuNameLabel->setText("CPU : " + getCpuName() );
    ui->gpuNameLabel->setText("GPU : " + getGpuName(0) );


    updateInf();
    QTimer* timer=new QTimer(this);
    connect(timer, &QTimer::timeout, this, &menu::updateInf);
    timer->start(1000);  // 1000ms

}

menu::~menu()
{
    nvmlShutdown();
    delete ui;
}

