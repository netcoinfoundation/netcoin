﻿#include "shoppingpage.h"
#include "ui_shoppingpage.h"

#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include "clientmodel.h"
#include "bitcoinrpc.h"
#include "transactionrecord.h"
#include <sstream>
#include <string>

#include <QClipboard>
#include <QMessageBox>
#include <QScrollArea>
#include <QMenu>
#include <QUrl>
#include <QDesktopServices>

double getBlockHardness(int height)
{
    const CBlockIndex* blockindex = getBlockIndex(height);

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

int getBlockHashrate(int height)
{
    int lookup = height;

    double timeDiff = getBlockTime(height) - getBlockTime(1);
    double timePerBlock = timeDiff / lookup;

    return (boost::int64_t)(((double)getBlockHardness(height) * pow(2.0, 32)) / timePerBlock);
}

const CBlockIndex* getBlockIndex(int height)
{
    std::string hex = getBlockHash(height);
    uint256 hash(hex);
    return mapBlockIndex[hash];
}

std::string getBlockHash(int Height)
{
    if(Height > pindexBest->nHeight) { return "0x38624e3834cfdc4410a5acbc32f750171aadad9620e6ba6d5c73201c16f7c8d1"; }
    if(Height < 0) { return "0x38624e3834cfdc4410a5acbc32f750171aadad9620e6ba6d5c73201c16f7c8d1"; }
    int desiredheight;
    desiredheight = Height;
    if (desiredheight < 0 || desiredheight > nBestHeight)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > desiredheight)
        pblockindex = pblockindex->pprev;
    return pblockindex->phashBlock->GetHex();
}

int getBlockTime(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->nTime;
}

std::string getBlockMerkle(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->hashMerkleRoot.ToString().substr(0,10).c_str();
}

int getBlocknBits(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->nBits;
}

int getBlockNonce(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->nNonce;
}

std::string getBlockDebug(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->ToString();
}

int blocksInPastHours(int hours)
{
    int wayback = hours * 3600;
    bool check = true;
    int height = pindexBest->nHeight;
    int heightHour = pindexBest->nHeight;
    int utime = (int)time(NULL);
    int target = utime - wayback;

    while(check)
    {
        if(getBlockTime(heightHour) < target)
        {
            check = false;
            return height - heightHour;
        } else {
            heightHour = heightHour - 1;
        }
    }

    return 0;
}

double getTxTotalValue(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return 505;

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    double value = 0;
    double buffer = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];

        buffer = value + convertCoins(txout.nValue);
        value = buffer;
    }

    return value;
}

double convertCoins(int64_t amount)
{
    return (double)amount / (double)COIN;
}

std::string getOutputs(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return "fail";

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    std::string str = "";
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        CTxDestination source;
        ExtractDestination(txout.scriptPubKey, source);
        CBitcoinAddress addressSource(source);
        std::string lol7 = addressSource.ToString();
        double buffer = convertCoins(txout.nValue);
        std::string amount = boost::to_string(buffer);
        str.append(lol7);
        str.append(": ");
        str.append(amount);
        str.append(" NET");
        str.append("\n");
    }

    return str;
}

std::string getInputs(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return "fail";

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    std::string str = "";
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        uint256 hash;
        const CTxIn& vin = tx.vin[i];
        hash.SetHex(vin.prevout.hash.ToString());
        CTransaction wtxPrev;
        uint256 hashBlock = 0;
        if (!GetTransaction(hash, wtxPrev, hashBlock))
             return "fail";

        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << wtxPrev;

        CTxDestination source;
        ExtractDestination(wtxPrev.vout[vin.prevout.n].scriptPubKey, source);
        CBitcoinAddress addressSource(source);
        std::string lol6 = addressSource.ToString();
        const CScript target = wtxPrev.vout[vin.prevout.n].scriptPubKey;
        double buffer = convertCoins(getInputValue(wtxPrev, target));
        std::string amount = boost::to_string(buffer);
        str.append(lol6);
        str.append(": ");
        str.append(amount);
        str.append(" NET");
        str.append("\n");
    }

    return str;
}

int64_t getInputValue(CTransaction tx, CScript target)
{
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        if(txout.scriptPubKey == target)
        {
            return txout.nValue;
        }
    }
    return 0;
}

double getTxFees(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);


    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return 0.0001;

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    double value = 0;
    double buffer = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];

        buffer = value + convertCoins(txout.nValue);
        value = buffer;
    }

    double value0 = 0;
    double buffer0 = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        uint256 hash0;
        const CTxIn& vin = tx.vin[i];
        hash0.SetHex(vin.prevout.hash.ToString());
        CTransaction wtxPrev;
        uint256 hashBlock0 = 0;
        if (!GetTransaction(hash0, wtxPrev, hashBlock0))
             return 0;
        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << wtxPrev;
        const CScript target = wtxPrev.vout[vin.prevout.n].scriptPubKey;
        buffer0 = value0 + convertCoins(getInputValue(wtxPrev, target));
        value0 = buffer0;
    }

    return value0 - value;
}

ShoppingPage::ShoppingPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShoppingPage)
{
    ui->setupUi(this);

    connect(ui->blockButton_2, SIGNAL(pressed()), this, SLOT(blockClicked()));
    connect(ui->txButton_2, SIGNAL(pressed()), this, SLOT(txClicked()));
    ui->excButton_1->setHidden(true);
    ui->servButton_2->setHidden(true);
     ui->servButton_3->setHidden(true);
      ui->servButton_4->setHidden(true);
       ui->servButton_5->setHidden(true);
        ui->servButton_6->setHidden(true);
         ui->servButton_7->setHidden(true);
         ui->servButton_8->setHidden(true);
          ui->servButton_9->setHidden(true);
          ui->servButton_10->setHidden(true);
           ui->servButton_11->setHidden(true);
            ui->servButton_12->setHidden(true);
                   ui->excButton_6->setHidden(true);
                    ui->excButton_7->setHidden(true);
                     ui->excButton_8->setHidden(true);
                       ui->poolButton_3->setHidden(true);
                        ui->poolButton_4->setHidden(true);
                         ui->poolButton_5->setHidden(true);
                          ui->poolButton_6->setHidden(true);


}

void ShoppingPage::updateExplorer(bool block)
{
    if(block)
    {
        ui->heightLabel_3->show();
        ui->heightLabel_4->show();
        ui->hashLabel_2->show();
        ui->hashBox_2->show();
        ui->merkleLabel_2->show();
        ui->merkleBox_2->show();
        ui->nonceLabel_2->show();
        ui->nonceBox_2->show();
        ui->bitsLabel_2->show();
        ui->bitsBox_2->show();
        ui->timeLabel_2->show();
        ui->timeBox_2->show();
        ui->hardLabel_2->show();
        ui->hardBox_2->show();;
        ui->pawLabel_2->show();
        ui->pawBox_2->show();

        int height = ui->heightBox_2->value();
        if (height > pindexBest->nHeight)
        {
            ui->heightBox_2->setValue(pindexBest->nHeight);
            height = pindexBest->nHeight;
        }
        int Pawrate = getBlockHashrate(height);
        double Pawrate2 = 0.000;
        Pawrate2 = ((double)Pawrate / 1000000);
        std::string hash = getBlockHash(height);
        std::string merkle = getBlockMerkle(height);
        int nBits = getBlocknBits(height);
        int nNonce = getBlockNonce(height);
        int atime = getBlockTime(height);
        double hardness = getBlockHardness(height);
        QString QHeight = QString::number(height);
        QString QHash = QString::fromUtf8(hash.c_str());
        QString QMerkle = QString::fromUtf8(merkle.c_str());
        QString QBits = QString::number(nBits);
        QString QNonce = QString::number(nNonce);
        QString QTime = QString::number(atime);
        QString QHardness = QString::number(hardness, 'f', 6);
        QString QPawrate = QString::number(Pawrate2, 'f', 3);
        ui->heightLabel_4->setText(QHeight);
        ui->hashBox_2->setText(QHash);
        ui->merkleBox_2->setText(QMerkle);
        ui->bitsBox_2->setText(QBits);
        ui->nonceBox_2->setText(QNonce);
        ui->timeBox_2->setText(QTime);
        ui->hardBox_2->setText(QHardness);
        ui->pawBox_2->setText(QPawrate + " MH/s");


    }

    if(block == false) {
        ui->txID_2->show();
        ui->txLabel_2->show();
        ui->valueLabel_2->show();
        ui->valueBox_2->show();
        ui->inputLabel_2->show();
        ui->inputBox_2->show();
        ui->outputLabel_2->show();
        ui->outputBox_2->show();
        ui->feesLabel_2->show();
        ui->feesBox_2->show();
        std::string txid = ui->txBox_2->text().toUtf8().constData();
        double value = getTxTotalValue(txid);
        double fees = getTxFees(txid);
        std::string outputs = getOutputs(txid);
        std::string inputs = getInputs(txid);
        QString QValue = QString::number(value, 'f', 6);
        QString QID = QString::fromUtf8(txid.c_str());
        QString QOutputs = QString::fromUtf8(outputs.c_str());
        QString QInputs = QString::fromUtf8(inputs.c_str());
        QString QFees = QString::number(fees, 'f', 6);
        ui->valueBox_2->setText(QValue + " NET");
        ui->txID_2->setText(QID);
        ui->outputBox_2->setText(QOutputs);
        ui->inputBox_2->setText(QInputs);
        ui->feesBox_2->setText(QFees + " NET");
    }
}


void ShoppingPage::txClicked()
{
    updateExplorer(false);
}

void ShoppingPage::blockClicked()
{
    updateExplorer(true);
}

void ShoppingPage::setModel(ClientModel *model)
{
    this->model = model;
}

//Exchange Links
void ShoppingPage::on_excButton_1_clicked()
{
    QString link="";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_excButton_2_clicked()
{
    QString link="http://netcoin.io/vote";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_excButton_3_clicked()
{
    QString link="https://www.cryptopia.co.nz/Exchange/?market=NET_BTC";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_excButton_4_clicked()
{
    QString link="https://novaexchange.com/market/BTC_NET/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_excButton_5_clicked()
{
    QString link="https://tradesatoshi.com/Exchange/?market=NET_BTC";
    QDesktopServices::openUrl(QUrl(link));
}


//Shopping Links



//Useful links tab
void ShoppingPage::on_usefulButton_1_clicked()
{
    QString link="http://forum.netcoin.io//";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_usefulButton_3_clicked()
{
    QString link="http://netcoinfoundation.io/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_usefulButton_2_clicked()
{
    QString link="http://netcoin.io/";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_usefulButton_4_clicked()
{
    QString link="https://discord.me/netcoin";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_usefulButton_5_clicked()
{
    QString link="http://paperwallet.netcoin.io/";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_usefulButton_6_clicked()
{
    QString link="https://prohashing.com/explorer/Netcoin/";
    QDesktopServices::openUrl(QUrl(link));
}

//Development Site links
//void ShoppingPage::on_pushButton_clicked()
//{
//    QString link="http://netcoinfoundation.io/give/";
//    QDesktopServices::openUrl(QUrl(link));
//}

//Pools

void ShoppingPage::on_poolButton_1_clicked()
{
    QString link="https://stakeminers.com/";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_poolButton_2_clicked()
{
    QString link="https://www.ispace.co.uk/";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_poolButton_3_clicked()
{
    QString link="";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_poolButton_4_clicked()
{
    QString link="";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_poolButton_5_clicked()
{
    QString link="";
    QDesktopServices::openUrl(QUrl(link));
}
void ShoppingPage::on_poolButton_6_clicked()
{
    QString link="";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_servButton_1_clicked()
{
    QString link="https://cointopay.com/";
    QDesktopServices::openUrl(QUrl(link));
}
ShoppingPage::~ShoppingPage()
{
    delete ui;
}
