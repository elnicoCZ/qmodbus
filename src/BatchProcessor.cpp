/*
 * BatchProcessor.cpp - implementation of BatchProcessor class
 *
 * Copyright (c) 2011-2014 Tobias Doerffel
 * Copyright (c) 2017      Petr Kubiznak
 *
 * This file is part of QModBus - http://qmodbus.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>

#include <errno.h>

#include "BatchProcessor.h"
#include "modbus-private.h"
#include "ui_BatchProcessor.h"

//******************************************************************************

BatchProcessor::BatchProcessor(QWidget *parent, modbus_t *modbus) :
  QDialog( parent ),
  ui( new Ui::BatchProcessor ),
  m_modbus( modbus ),
  m_timer(),
  m_oInputDir(),
  m_oInputMenu(this)
{
  ui->setupUi(this);

  connect(&m_timer, SIGNAL(timeout()), this, SLOT(runBatch()));

  ui->batchLoadBtn->setMenu(&m_oInputMenu);
  updateBatchFileMenu();
  connect(&m_oInputMenu, SIGNAL(triggered(QAction*)),
          this, SLOT(batchMenuTriggered(QAction*)));
}

//******************************************************************************

BatchProcessor::~BatchProcessor()
{
  stop();
  delete ui;
}

//******************************************************************************

void BatchProcessor::start()
{
  if (!validateBatch())
  {
    QMessageBox::critical(this,
                          tr("Invalid command"),
                          tr("Batch command parsing failed"));
    return;
  }

  logClose();
  logOpen(ui->outputFileEdit->text());

  runBatch();

  int iPeriod = ui->intervalSpinBox->value();
  if (iPeriod > 0)
  {
    m_timer.start(iPeriod * 1000);

    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
  }
}

//******************************************************************************

void BatchProcessor::stop()
{
  logClose();

  m_timer.stop();

  ui->startButton->setEnabled(true);
  ui->stopButton->setEnabled(false);
}

//******************************************************************************

void BatchProcessor::browseOutputFile()
{
  QString fileName = QFileDialog::getSaveFileName(this,
                                                  tr("Get output file"),
                                                  QString(),
                                                  tr("CSV files (*.csv)"));
  if (!fileName.isEmpty())
  {
    ui->outputFileEdit->setText(fileName);
  }
}

//******************************************************************************

void BatchProcessor::browseBatchFile()
{
  QString sFilename = QFileDialog::getOpenFileName(this,
                                                   tr("Get batch file"),
                                                   QString(),
                                                   tr("QModBus Batch (*.qmb)"));
  loadBatchFile(sFilename);
}

//******************************************************************************

bool BatchProcessor::loadBatchFile(const QString & sFilename)
{
  if (sFilename.isEmpty()) return false;

  QFile qFile(sFilename);
  if (!qFile.open(QIODevice::ReadOnly))
  {
    QMessageBox::warning(this,
                         tr("Could not open file"),
                         tr("Could not open batch file %1 for reading.")
                            .arg(m_oOutputFile.fileName()));
    return false;
  }

  ui->batchEdit->setPlainText(qFile.readAll());
  this->updateBatchFileMenu(QFileInfo(sFilename).absolutePath());

  return true;
}

//******************************************************************************

void BatchProcessor::runBatch()
{
  processBatch(ui->batchEdit->toPlainText(), true);
}

//******************************************************************************

bool BatchProcessor::validateBatch()
{
  return processBatch(ui->batchEdit->toPlainText(), false);
}

//******************************************************************************

bool BatchProcessor::processBatch(const QString & qBatch, bool bExecute)
{
  bool              bParseSucc = true;
  const QStringList qSlaves    = qBatch.split( ';' );

  foreach( const QString & qSlave, qSlaves )
  {
    const QString     qCommand = qSlave.split(':').first();
    const QStringList qDatas   = qSlave.split(':').last ().split(',');
    bool              bSucc    = true;

    const int         iSlaveId = qCommand.split('x').first().toInt(&bSucc);     bParseSucc &= bSucc;
    const int         iFuncId  = qCommand.split('x').last ().toInt(&bSucc, 16); bParseSucc &= bSucc;

    foreach( const QString & qData, qDatas )
    {
      const QStringList qAddrVal = qData.split('=');
      const int iAddr = qAddrVal.first().toInt(&bSucc); bParseSucc &= bSucc;
      const int iVal  = qAddrVal.last ().toInt(&bSucc); bParseSucc &= bSucc;

      switch (getFuncType(iFuncId))
      {
        case neFuncTypeRead:
          bParseSucc &= (qAddrVal.count() == 1);
          break;

        case neFuncTypeWrite:
          bParseSucc &= (qAddrVal.count() == 2);
          break;

        default:
          bParseSucc = false;
          break;
      }

      if (bParseSucc && bExecute)
      {
        execRequest(iSlaveId, iFuncId, iAddr, iVal);
      }
    }
  }

  return bParseSucc;
}

//******************************************************************************

BatchProcessor::EFuncType BatchProcessor::getFuncType(int iFuncId)
{
  switch (iFuncId)
  {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS:
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS:
      return neFuncTypeRead;

    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_SINGLE_REGISTER:
    case MODBUS_FC_WRITE_MULTIPLE_COILS:
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
      return neFuncTypeWrite;

    default:
      return neFuncTypeInvalid;
  }
}

//******************************************************************************

void BatchProcessor::execRequest(int            iSlaveId,
                                 int            iFuncId,
                                 int            iAddr,
                                 int            iVal)
{
  QString qStr;
  QTextStream qStrStream(&qStr);

  qStrStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << ", "
             << iSlaveId << ", "
             << "0x" << QString::number(iFuncId, 16).toUpper() << ", "
             << iAddr << ", "
             << sendModbusRequest(iSlaveId, iFuncId, iAddr, iVal);

  logWrite(qStr);
}

//******************************************************************************

QString BatchProcessor::sendModbusRequest(int iSlaveID,
                                          int iFuncId,
                                          int iAddr,
                                          int iVal)
{
  if (m_modbus == NULL)
  {
    return QString();
  }

  const int num = 1;
  uint8_t dest[num*sizeof(uint16_t)];
  uint16_t * dest16 = (uint16_t *) dest;

  memset(dest, 0, sizeof(dest));

  int ret = -1;
  bool is16Bit = false;

  modbus_set_slave(m_modbus, iSlaveID);

  switch (iFuncId)
  {
    case MODBUS_FC_READ_COILS:
      ret = modbus_read_bits(m_modbus, iAddr, num, dest);
      break;

    case MODBUS_FC_READ_DISCRETE_INPUTS:
      ret = modbus_read_input_bits(m_modbus, iAddr, num, dest);
      break;

    case MODBUS_FC_READ_HOLDING_REGISTERS:
      ret = modbus_read_registers(m_modbus, iAddr, num, dest16);
      is16Bit = true;
      break;

    case MODBUS_FC_READ_INPUT_REGISTERS:
      ret = modbus_read_input_registers(m_modbus, iAddr, num, dest16);
      is16Bit = true;
      break;

    case MODBUS_FC_WRITE_SINGLE_COIL:
      ret = modbus_write_bit(m_modbus, iAddr, iVal);
      break;

    case MODBUS_FC_WRITE_SINGLE_REGISTER:
      ret = modbus_write_register(m_modbus, iAddr, iVal);
      break;

    case MODBUS_FC_WRITE_MULTIPLE_COILS:
    {
      uint8_t * au8Data = new uint8_t[num];
      for (int i = 0; i < num; ++i)
      {
        au8Data[i] = iVal;
      }
      ret = modbus_write_bits(m_modbus, iAddr, num, au8Data);
      delete[] au8Data;
      break;
    }

    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
    {
      uint16_t * au16Data = new uint16_t[num];
      for (int i = 0; i < num; ++i)
      {
        au16Data[i] = iVal;
      }
      ret = modbus_write_registers(m_modbus, iAddr, num, au16Data);
      delete[] au16Data;
      break;
    }

    default:
      // should not happen, as we validate the batch prior to execution
      QMessageBox::warning(this, tr("Unimplemented function code"),
                           tr("Function code %1 not implemented").arg(iFuncId));
      break;
  }

  if (ret == num)
  {
    bool b_hex = false;//is16Bit && ui->checkBoxHexData->checkState() == Qt::Checked;
    QString qs_num;

    for( int i = 0; i < num; ++i )
    {
      int data = is16Bit ? dest16[i] : dest[i];

      qs_num += QString().sprintf( b_hex ? "0x%04x" : "%d", data);
    }

    return qs_num;
  }
  else
  {
    if (ret < 0)
    {
      if (
#ifdef WIN32
          errno == WSAETIMEDOUT ||
#endif
          errno == EIO
                                  )
      {
        return tr("-1 (I/O error: did not receive any data from slave.)");
      }
      else
      {
        return tr("-1 (Slave threw exception \"%1\" or function not implemented.)")
                  .arg(modbus_strerror(errno));
      }
    }
    else
    {
      return tr("-1 (Number of registers returned does not match "
                "number of registers requested!)");
    }
  }

  return "-1 (NO VALID DATA RECEIVED)";
}

//******************************************************************************

void BatchProcessor::logOpen(const QString & sFilename)
{
  ui->txtLog->clear();

  if (sFilename.isEmpty()) return;

  m_oOutputFile.setFileName(sFilename);
  if(!m_oOutputFile.open(QFile::WriteOnly | QFile::Truncate))
  {
    QMessageBox::critical(this,
                          tr("Could not open file"),
                          tr("Could not open output file %1 for writing.")
                            .arg(m_oOutputFile.fileName()));
  }
}

//******************************************************************************

void BatchProcessor::logWrite(const QString & qStr)
{
  ui->txtLog->appendPlainText(qStr);

  if (m_oOutputFile.isOpen())
  {
    QTextStream qFileStream(&m_oOutputFile);
    qFileStream << qStr << endl;
  }
}

//******************************************************************************

void BatchProcessor::logClose(void)
{
  m_oOutputFile.close();
}

//******************************************************************************

void BatchProcessor::updateBatchFileMenu(const QString & sPath)
{
  if (!sPath.isEmpty()) m_oInputDir = QDir(sPath);

  m_oInputMenu.clear();
  QStringList qasFiles = m_oInputDir.entryList(QStringList("*.qmb"),
                                               QDir::Files | QDir::Readable,
                                               QDir::Name);
  foreach(const QString & qsFile, qasFiles)
  {
    m_oInputMenu.addAction(qsFile);
  }

}

//******************************************************************************

void BatchProcessor::batchMenuTriggered(QAction * qAction)
{
  loadBatchFile(m_oInputDir.absolutePath() + "/" + qAction->text());
}

//******************************************************************************
