#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include <QTableWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    QPushButton *loadButton;
    QPushButton *encodeButton;

    QByteArray buffer;
    QVector<int> freq;
    QMultiMap<int, QByteArray> toDo;
    QMap<QByteArray, QPair<QByteArray, QByteArray> > children;
    QByteArray rootNode;


public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTableWidget *table;

    void clearContents();
    void clickedLoadButton();
    void clickedEncodeButton();
    void clickedDecodeButton();
    void saveTrueEncoding(const QByteArray &encodedData, int nBits);
    QString unpackBits(const QByteArray encodedData, int numBits);
    QByteArray decodeHuffman(const QByteArray &encodedData, int numBits);


};
#endif // MAINWINDOW_H
