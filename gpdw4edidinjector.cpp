#include <QPointer>
#include <QMessageBox>
#include <QFileDialog>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include <QInputDialog>
#include <QLabel>

#include "gpdw4edidinjector.h"
#include "./ui_gpdw4edidinjector.h"

GpdW4EdidInjector::GpdW4EdidInjector(QWidget *parent): QMainWindow(parent), ui(new Ui::GpdW4EdidInjector) {
    ui->setupUi(this);

    QPointer<QAction> menuOpenHex {ui->menuFile->actions().at(0)};
    QPointer<QAction> menuSaveHex {ui->menuFile->actions().at(1)};
    QPointer<QAction> menuExtractFw {ui->menuExtract->actions().at(0)};
    QPointer<QAction> menuExtractEdid {ui->menuExtract->actions().at(1)};
    QPointer<QAction> menuAdvSetEdidOff {ui->menuAdvanced->actions().at(0)};
    QPointer<QAction> menuAdvSetEdidSz {ui->menuAdvanced->actions().at(1)};
    QPointer<QAction> menuAdvResetDef {ui->menuAdvanced->actions().at(2)};
    QPointer<QAction> menuInjEdid {ui->menuInject->actions().at(0)};

    qhexWidget = new QHexView();

    setHexView(); // workaround stupid widget
    ui->fwHexLayt->addWidget(qhexWidget);
    ui->tabWidget->setCurrentIndex(Tabs::Firmware);
    ui->statusbar->addWidget(new QLabel("Made by Awx - GPLv3"));

    connect(menuOpenHex, &QAction::triggered, this, &GpdW4EdidInjector::onOpenHex);
    connect(menuSaveHex, &QAction::triggered, this, &GpdW4EdidInjector::onSaveHex);
    connect(menuExtractFw, &QAction::triggered, this, &GpdW4EdidInjector::onExtractFw);
    connect(menuExtractEdid, &QAction::triggered, this, &GpdW4EdidInjector::onExtractEdid);
    connect(menuAdvSetEdidOff, &QAction::triggered, this, &GpdW4EdidInjector::onSetEdidOffset);
    connect(menuAdvSetEdidSz, &QAction::triggered, this, &GpdW4EdidInjector::onSetEdidSize);
    connect(menuAdvResetDef, &QAction::triggered, this, &GpdW4EdidInjector::onEdidResetToDefaults);
    connect(menuInjEdid, &QAction::triggered, this, &GpdW4EdidInjector::onEdidInjection);
}

GpdW4EdidInjector::~GpdW4EdidInjector() {
    delete ui;
}

void GpdW4EdidInjector::onOpenHex() {
    QString hexPath { QFileDialog::getOpenFileName(this, "Open .hex firmware", nullptr, "Intel HEX (*.hex)") };

    if (hexPath.isNull())
        return;

    ui->logsTxtBox->clear();
    parseErrors.clear();
    loadFw(hexPath);
    writeLogs();

    if (parseErrors.size() > 0) {
        ui->statusbar->showMessage(QString("%1: firmware file has errors").arg(hexPath));

    } else {
        setHexView();
        ui->statusbar->showMessage(hexPath);
    }
}

void GpdW4EdidInjector::onSaveHex() {
    QString outFile { QFileDialog::getSaveFileName(this, QString("Save custom LCD firmware"), QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/custom_win4-LCD_1080x1920_firmware.hex"), tr("Intel HEX (*.hex)")) };
    QFile out;
    qint64 ret;

    if (outFile.isNull())
        return;

    out.setFileName(outFile);

    if (!out.open(QIODevice::WriteOnly)) {
        errorMsg("Unable to write file", "Error");
        return;
    }

    for (qsizetype i=0,l=firmwareData.size(); i<l; i+=16) {
        qsizetype sliceSz = (i + 16) > l ? (l - i) : 16;
        QByteArray data = firmwareData.sliced(i, sliceSz);
        qsizetype bytesC = data.size();

        // add type
        data.prepend(QByteArray::fromHex(QString("00").toUtf8()));

        // add address
        data.prepend(QByteArray::fromHex(QString::number(i, 16).rightJustified(4, '0').toUtf8()));

        // add bytes count
        data.prepend(QByteArray::fromHex(QString::number(bytesC, 16).rightJustified(2, '0').toUtf8()));

        // add checksum
        data.append(QByteArray::fromHex(getRecordChecksum(data).toUtf8()));

        ret = out.write(":" + data.toHex().toUpper() + "\r\n");
        if (ret == -1)
            break;
    }

    if (ret != -1)
        out.write(":00000001FF"); // eof

    out.close();

    if (ret == -1)
        errorMsg("Failed to write to file", "Error");
    else
        successMsg("Saved!", "Success");
}

void GpdW4EdidInjector::onExtractEdid() {
    if (firmwareData.isEmpty()) {
        errorMsg("No firmware is currently loaded", "Error");
        return;
    }

    exportBin("edid.bin", firmwareData.sliced(edidOff, edidSz));
}

void GpdW4EdidInjector::onExtractFw() {
    if (firmwareData.isEmpty()) {
        errorMsg("No firmware is currently loaded", "Error");
        return;
    }

    exportBin("firmware.bin", firmwareData);
}

void GpdW4EdidInjector::onSetEdidOffset() {
    bool ok;
    QString inp { QInputDialog::getText(this, "Set edid start offset", "Start offset:", QLineEdit::Normal, "0x" + QString::number(edidOff, 16).toUpper(), &ok) };

    if (ok && !inp.isEmpty()) {
        edidOff = inp.toULong(&ok, 16);

        if (!ok)
            errorMsg("Failed to save value, please reset to defaults", "Error");
    }
}

void GpdW4EdidInjector::onSetEdidSize() {
    bool ok;
    QString inp { QInputDialog::getText(this, "Set edid size", "Size (bytes):", QLineEdit::Normal, QString::number(edidSz, 10).toUpper(), &ok) };

    if (ok && !inp.isEmpty()) {
        edidSz = inp.toUInt(&ok, 10);

        if (!ok)
            errorMsg("Failed to save value, please reset to defaults", "Error");
    }
}

void GpdW4EdidInjector::onEdidResetToDefaults() {
    edidOff = 0x1AC1;
    edidSz = 128;
}

void GpdW4EdidInjector::onEdidInjection() {
    if (firmwareData.isEmpty()) {
        errorMsg("No firmware is currently loaded", "Error");
        return;
    }

    QString filePath { QFileDialog::getOpenFileName(this, "Load edid", QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)), "Edid binary (*.bin)") };
    QByteArray edidData;
    QFile edid;

    if (filePath.isNull())
        return;

    edid.setFileName(filePath);

    if (!edid.open(QIODevice::ReadOnly)) {
        errorMsg("Failed to read edid file", "Error");
        return;
    }

    edidData = edid.readAll();

    edid.close();

    if ((edidData.size() % 128) != 0) {
        errorMsg("Selected edid is not valid", "Error");
        return;
    }

    firmwareData.replace(edidOff, edidData.size(), edidData);
    setHexView();

    edidSz = edidData.size(); // update edid size

    successMsg("Injected!", "Success");
}

void GpdW4EdidInjector::loadFw(const QString &path) {
    qint32 lineNo = 1;
    QFile f(path);
    QTextStream fstream;
    QString line;

    if (!f.open(QIODevice::ReadOnly)) {
        errorMsg("Failed to read hex file", "Error");
        return;
    }

    firmwareData.clear();
    fstream.setDevice(&f);

    line = fstream.readLine();
    while (!line.isNull()) {
        parseIHexLine(line, lineNo);

        line = fstream.readLine();
        ++lineNo;
    }

    f.close();
}

void GpdW4EdidInjector::parseIHexLine(const QString &line, qint32 lineNo) {
    QRegularExpressionMatch match;
    QString compCsum;

    if (line.at(0) != ':') {
        parseErrors.append(QString("line %1: invalid start code").arg(lineNo));
        return;
    }

    match = ihexEOFRegex.match(line);
    if (match.hasMatch())
        return;

    match = ihexLineRegex.match(line);
    if (!match.hasMatch()) {
        parseErrors.append(QString("line %1: invalid record").arg(lineNo));
        return;
    }

    firmwareData.append(QByteArray::fromHex(match.captured("data").toUtf8()));

    compCsum = getRecordChecksum(QByteArray::fromHex( QString(match.captured("bytescount") + match.captured("addr") + match.captured("rectype") + match.captured("data")).toUtf8() ));

    if (compCsum.compare(match.captured("csum"), Qt::CaseInsensitive) != 0)
        parseErrors.append(QString("line %1: checksum mismatch").arg(lineNo));
}

void GpdW4EdidInjector::exportBin(const QString &filename, const QByteArray &data) {
    QString outFile { QFileDialog::getSaveFileName(this, QString("Export " + filename), QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/" + filename), "Binary (*.bin)") };
    QFile out;
    qint64 ret;

    if (outFile.isNull())
        return;

    out.setFileName(outFile);

    if (!out.open(QIODevice::WriteOnly)) {
        errorMsg("Failed to write to file", "Error");
        return;
    }

    ret = out.write(data);

    out.close();

    if (ret == -1)
        errorMsg("Failed to write to file", "Error");
    else
        successMsg("Saved!", "Success");
}

QString GpdW4EdidInjector::getRecordChecksum(const QByteArray &ba) const {
    qint32 sum = 0;
    uchar ret = 0;

    for (int i=0,l=ba.size(); i<l; ++i)
        sum += ba.at(i);

    ret = -(uint)sum;

    return QString::number(ret, 16).rightJustified(2, '0');
}

void GpdW4EdidInjector::setHexView() const {
    QHexView::DataStorageArray *dsa { new QHexView::DataStorageArray(firmwareData) };

    qhexWidget->clear();
    qhexWidget->setData(dsa);
}

void GpdW4EdidInjector::writeLogs() const {
    for (const QString &error : parseErrors)
        ui->logsTxtBox->appendPlainText(error);

    ui->tabWidget->setTabText(Tabs::Logs, "Logs (" + QString::number(parseErrors.size()) + ")");
}

void GpdW4EdidInjector::errorMsg(const QString &msg, const QString &title) {
    QMessageBox::critical(this, title, msg, QMessageBox::Ok);
}

void GpdW4EdidInjector::successMsg(const QString &msg, const QString &title) {
    QMessageBox::information(this, title, msg, QMessageBox::Ok);
}
