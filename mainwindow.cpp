#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtWidgets>
#include <QFileDialog>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QIODevice>
#include <QDataStream>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    ,freq(256,0)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QWidget *window = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(window);
    setMenuWidget(window);

    /* for now prompt for a file and open it */
    QPushButton *loadButton = new QPushButton("Load");
    layout->addWidget(loadButton);
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::clickedLoadButton);

    /* encode the file opened */
    QPushButton *encodeButton = new QPushButton("Encode");
    layout->addWidget(encodeButton);
    connect(encodeButton, &QPushButton::clicked, this, &MainWindow::clickedEncodeButton);

    /* load the binary file */
    QPushButton *decodeButton = new QPushButton("Decode");
    layout->addWidget(decodeButton);
    connect(decodeButton, &QPushButton::clicked, this, &MainWindow::clickedDecodeButton);

    table = new QTableWidget(this);
    setCentralWidget(table);

    int nRows=255; int nCols=4;
    QStringList headers;
    headers << "Character Code" << "Symbol" << "Frequencies" << "Huffman Encoding";
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(nRows);
    table->setColumnCount(nCols);
}

void MainWindow::clearContents() {
    table->clearContents();

}


/* Prompt for a file and open it */
void MainWindow::clickedLoadButton() {

    QString fName = QFileDialog::getOpenFileName(this, "please slect file to open");

    if(fName.isEmpty()) return;

    QFile inFile(fName);
    if(!inFile.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, "Error", QString("Cant open file \"%l\"").arg(fName));
        return;
    }

    buffer = inFile.readAll();
    inFile.close();

    /* warn user if no content in file */
    if (buffer.isEmpty()) {
        QMessageBox::information(this, "Error", QString("There is no data to be loaded from this file \"%1\"").arg(fName));
        return;
    }

    /* clear contents before loading any new data */
    table->clearContents();
    /* reset freq array */
    freq.fill(0);

    for(int i=0; i < buffer.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(buffer[i]);
        ++freq[byte];
    }

    /* fill in table based on our table headers */
    for(int i=0; i < 256; ++i) {
        if(freq[i] > 0) {
            QTableWidgetItem *charCodeItem = new QTableWidgetItem(QString::number(i));
            table->setItem(i, 0, charCodeItem);

            QTableWidgetItem *symbolItem = new QTableWidgetItem(QString(QChar(i)));
            table->setItem(i, 1, symbolItem);

            QTableWidgetItem *occurItem = new QTableWidgetItem(QString::number(freq[i]));
            table->setItem(i, 2, occurItem);

            /* make sure these row appear in our table */
            table->setRowHidden(i, false);
        }
        /* hide any unused rows */
        else {
            table->setRowHidden(i,true);
        }
    }
}


/* move through bytes */
void MainWindow::clickedEncodeButton() {

    for(int i=0; i < 256; ++i) {
        if(freq[i] > 0) {
            QByteArray character(1, static_cast<char>(i));
            toDo.insert(freq[i], character);
        }
    }

    QVector<QString> charCodeEncodings(256, "");

    /* handle a file with only one symbol */
    if(toDo.size() == 1) {
        QByteArray oneSymbol = toDo.begin().value();
        /* set one symbol to a 0 */
        charCodeEncodings[oneSymbol[0]] = "0";

        QTableWidgetItem *huffmanItem = new QTableWidgetItem("0");
        table->setItem(oneSymbol[0], 3, huffmanItem);

        /* dont have to build a tree */
        return;
    }

    /* build our huffman tree */
    while(toDo.size() > 1) {

        int freq0 = toDo.begin().key();
        QByteArray char0 = toDo.begin().value();
        toDo.erase(toDo.begin());

        int freq1 = toDo.begin().key();
        QByteArray char1 = toDo.begin().value();
        toDo.erase(toDo.begin());

        int parentFreq =  freq0 + freq1;
        QByteArray parentChar = char0 + char1;

        toDo.insert(parentFreq, parentChar);
        children[parentChar] = qMakePair(char0, char1);
    }

    /* root of our huffman tree */
    rootNode = toDo.begin().value();

    /* generate codes from huffman */
    for(int i=0; i < 256; ++i) {
        if(freq[i] > 0) {
            /* the charcater */
            QByteArray target(1, static_cast<char>(i));

            /* start at the root node */
            QByteArray current = rootNode;
            /* empty for now */
            QString code = "";

            /* traverse */
            while(current != target) {

                if(children[current].first.contains(target)) {
                    /* append 0 when moving left and then move to the left child */
                    code.append("0");
                    current = children[current].first;
                }
                else if(children[current].second.contains(target)) {
                     /* append 0 when moving right and then move to the right child */
                    code.append("1");
                    current = children[current].second;
                }
            }

            /* store code and check */
            charCodeEncodings[i] = code;

            /* update table with our string encoding */
            QTableWidgetItem *huffmanItem = new QTableWidgetItem(code);
            table->setItem(i,3, huffmanItem);
        }
    }

    /* get our true binary encoding using QByteArray */
    QByteArray trueEncoding;
    int numBits = 0;

    /* estimate our encdoing size to avoid overflow */
    int estimatedEncodingSize = (buffer.size() * 8) /8 + 1;
    trueEncoding.resize(estimatedEncodingSize);

    for(int i=0; i < buffer.size(); ++i) {
        QString huffmanCode = charCodeEncodings[(unsigned char)buffer[i]];

        /* transfer our huffman code into byte array */
        for(int j=0; j < huffmanCode.size(); ++j) {
            if(huffmanCode[j] == '1') {
                /* 8 bit */
                if(numBits % 8 == 0) {
                    trueEncoding.append(char(0));
                }

                /* if our encoding doesnt fit perfectly into chunks of 8bits */
                char currentByte = trueEncoding[numBits/ 8];
                int bitPosition = 7-(numBits % 8); /* calc left to right */
                char bitMask = (1 << bitPosition);
                currentByte = currentByte | bitMask;
                trueEncoding[numBits/ 8] = currentByte;

            }
            numBits++;
        }
    }
    /* resize our encoding to the actual size */
    trueEncoding.resize((numBits + 7) / 8);

    /* save data into a file */
    saveTrueEncoding(trueEncoding, numBits);
}


void MainWindow::saveTrueEncoding(const QByteArray &trueEncoding, int numBits) {

    QString fName = QFileDialog::getSaveFileName(this, tr("Save your encoded file"), "", tr("Binary Files (*.bin)"));

    if(fName.isEmpty()) return;

    QFile file(fName);
    if(!file.open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, "Error", QString("Cant open file \"%l\"").arg(fName));
        return;
    }

    QDataStream out(&file);
    out << numBits;
    /* write the encdoing and size of the file */
    out.writeRawData(trueEncoding.constData(), trueEncoding.size());

    file.close();

    quint64 orginialFileSize = buffer.size();
    quint64 encodedFileSize = QFileInfo(file).size();

    /* report file sizes to the user */
    QString message = QString("The original file size is: %1 bytes. \nThe next resulting file is %2 bytes.").arg(orginialFileSize).arg(encodedFileSize);
    QMessageBox::information(this,"File Sizes", message);
}


/* load our binary file as saved in Encode */
void MainWindow::clickedDecodeButton() {

    QString fileName = QFileDialog::getOpenFileName(this, tr("Open encoded file"), "", tr("All Files (*.*);;Binary files (*.bin)"));

    if(fileName.isEmpty()) return;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        /* fix this later to say actual name of the file */
        QMessageBox::information(this, "Error", QString("Cant open file \"%l\"").arg(fileName));
        return;
    }

    if (file.size() == 0) {
        QMessageBox::information(this, "Error", QString("The file \"%l\" is empty and cant be decoded").arg(fileName));
        file.close();
        return;
    }

    /* clear if file selected */
    clearContents();

    QDataStream in(&file);

    /* read in the number of bits */
    int numBits;
    in >> numBits;

    /* for reading in the encoded data */
    QByteArray encodedData((numBits + 7) / 8, 0);
    in.readRawData(encodedData.data(), encodedData.size());

    file.close();

    QByteArray decodedBinary = decodeHuffman(encodedData, numBits);
    QString saveFileName = QFileDialog::getSaveFileName(this, tr("Save decoded data"), "", tr("All files (*.*);;Binary Files (*.jpg *.png *.pdf)"));

    if(!saveFileName.isEmpty()) {
        QFile outFile(saveFileName);
        if(outFile.open(QIODevice::WriteOnly)) {
            outFile.write(decodedBinary);
            outFile.close();
        } else {
            QMessageBox::information(this, "Error", QString("Can't open file \"%1\"").arg(saveFileName));
        }
    }
}


QByteArray MainWindow::decodeHuffman(const QByteArray &encodedData, int numBits) {
    QByteArray current = rootNode;
    QByteArray decodedBinary;
    QVector<int> decodedFreq(256, 0);

    for(int i = 0; i < numBits; ++i) {
        if(encodedData[i / 8] & (1 << (7 - (i % 8)))) {
            if(!children.contains(current)) {
                qDebug() << "Error: Current node has no right child";
                break;
            }
            current = children[current].second;
        } else {
            if(!children.contains(current)) {
                qDebug() << "Error: Current node has no left child";
                break;
            }
            current = children[current].first;
        }

        /* check if current is a leaf node */
        if(!children.contains(current)) {
            decodedBinary.append(current);

            /* update table with decoded values */
            int symbolCode = static_cast<unsigned char>(current[0]);
            if(symbolCode < 256) {
                /* get occurences */
                decodedFreq[symbolCode]++;

                /* table updates */
                QTableWidgetItem *charCodeItem = new QTableWidgetItem(QString::number(symbolCode));
                table->setItem(symbolCode, 0, charCodeItem);

                QTableWidgetItem *symbolItem = new QTableWidgetItem(QString(QChar(symbolCode)));
                table->setItem(symbolCode, 1, symbolItem);

                QTableWidgetItem *occurItem = new QTableWidgetItem(QString::number(decodedFreq[symbolCode]));
                table->setItem(symbolCode, 2, occurItem);

                table->setRowHidden(symbolCode, false);
            }

            /* reset to root for next symbol */
            current = rootNode;
        }
    }
    return decodedBinary;
}


MainWindow::~MainWindow()
{
    delete ui;
}
