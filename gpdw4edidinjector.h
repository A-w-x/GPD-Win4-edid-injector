#ifndef GPDW4EDIDINJECTOR_H
#define GPDW4EDIDINJECTOR_H

#include <QMainWindow>
#include <QByteArray>
#include <QList>
#include <QRegularExpression>

#include "QHexView.h"

QT_BEGIN_NAMESPACE
namespace Ui { class GpdW4EdidInjector; }
QT_END_NAMESPACE

class GpdW4EdidInjector : public QMainWindow
{
    Q_OBJECT

private slots:
    void onOpenHex();
    void onSaveHex();
    void onExtractEdid();
    void onExtractFw();
    void onSetEdidOffset();
    void onSetEdidSize();
    void onEdidResetToDefaults();
    void onEdidInjection();

public:
    GpdW4EdidInjector(QWidget *parent = nullptr);
    ~GpdW4EdidInjector();

private:
    enum Tabs: int {
        Firmware = 0,
        Logs
    };

    Ui::GpdW4EdidInjector *ui;

    const QRegularExpression ihexLineRegex {"^:(?<bytescount>[0-9a-zA-Z]{2})(?<addr>[0-9a-zA-Z]{4})(?<rectype>[0-9a-zA-Z]{2})(?<data>[0-9a-zA-Z]+)(?<csum>[0-9a-zA-Z]{2})$"};
    const QRegularExpression ihexEOFRegex {"^:([0-9a-zA-Z]{2})([0-9a-zA-Z]{4})([0-9a-zA-Z]{2})([0-9a-zA-Z]{2})$"};
    QHexView *qhexWidget = nullptr;
    qsizetype edidOff = 0x1AC1;
    qint32 edidSz = 128; // bytes
    QByteArray firmwareData;
    QList<QString> parseErrors;

    void loadFw(const QString &path);
    void parseIHexLine(const QString &line, qint32 lineNo);
    void setHexView() const;
    void exportBin(const QString &filename, const QByteArray &data);
    QString getRecordChecksum(const QByteArray &ba) const;
    void writeLogs() const;
    void errorMsg(const QString &msg, const QString &title);
    void successMsg(const QString &msg, const QString &title);
};
#endif // GPDW4EDIDINJECTOR_H
