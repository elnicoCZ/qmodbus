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
#include <QTextBlock>

#include <errno.h>

#include "BatchProcessor.h"
#include "BatchParser.h"
#include "modbus-private.h"
#include "ui_BatchProcessor.h"

//******************************************************************************
//******************************************************************************
//******************************************************************************

BatchHighlighter::BatchHighlighter(QTextDocument * parent,
                                   Batch::CBatch & oBatch):
  QSyntaxHighlighter(parent),
  m_oBatch(oBatch)
{
  //
}

//******************************************************************************

void BatchHighlighter::highlightBlock(const QString & qsBlockText)
{
  // rebuild the model
  m_oBatch.rebuild(((QTextDocument*)parent())->toPlainText());

  int nBlockStart = currentBlock().position();
  int nBlockLen   = currentBlock().length();
  int nIdxStart   = m_oBatch.commandIndex(nBlockStart);
  int nIdxEnd     = m_oBatch.commandIndex(nBlockStart + nBlockLen - 1);

  for (int i=nIdxStart; i<=nIdxEnd; ++i)
  {
    const Batch::CCommand * poCommand = m_oBatch.at(i);
    if (!poCommand)
    {
      // should not happen
      qDebug() << "Highlighter: No command at index" << i;
      continue;
    }
    int nStart = poCommand->start() - nBlockStart;
    int nEnd_  = nStart + poCommand->len();                 // 1 char after the end
    if (nStart < 0) nStart = 0;                             // command starts in a previous block
    if (nEnd_ > nBlockLen) nEnd_ = nBlockLen;               // command ends in a following block

    // Select and apply the format
    QBrush qBrush;
    QTextCharFormat qFormat;
    switch (poCommand->type())
    {
      case Batch::neCommandComment   : qBrush = Qt::darkGray ; break;
      case Batch::neCommandDirective : qBrush = Qt::darkRed  ; break;
      case Batch::neCommandDelay     : qBrush = Qt::darkBlue ; break;
      case Batch::neCommandRequest   : qBrush = Qt::darkGreen; break;
      default                        : qBrush = Qt::black    ; break;
    }
    qFormat.setForeground(qBrush);
    if (!poCommand->valid()) qFormat.setBackground(Qt::red);
    setFormat(nStart, nEnd_-nStart, qFormat);

    // Keep the last command type in the block state.
    // If the value changes, the next block is automatically rehighlighted.
    setCurrentBlockState(poCommand->type() + (poCommand->valid() ? 0 : 100));
  }
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

BatchProcessor::BatchProcessor(QWidget *parent, modbus_t *modbus) :
  QDialog( parent ),
  ui( new Ui::BatchProcessor ),
  m_modbus( modbus ),
  m_timer(),
  m_oInputDir(),
  m_oInputMenu(this),
  m_oBatch(""),
  m_bStopAfterExecution(true)
{
  ui->setupUi(this);

  connect(&m_timer, SIGNAL(timeout()), this, SLOT(runBatch()));

  ui->batchLoadBtn->setMenu(&m_oInputMenu);
  updateBatchFileMenu();
  connect(&m_oInputMenu, SIGNAL(triggered         (QAction*)),
          this         , SLOT  (batchMenuTriggered(QAction*)));

  updateOutputFile();

  m_poBatchHighlighter = new BatchHighlighter(ui->batchEdit->document(),
                                              m_oBatch);

  connect(&m_oBatch, SIGNAL(execCommand     (int)),
          this     , SLOT  (highlightCommand(int)));
  connect(&m_oBatch, SIGNAL(changed()),
          this     , SLOT  (batchChanged()));
  connect(&m_oBatch, SIGNAL(execStart()),
          this     , SLOT  (execStart()));
  connect(&m_oBatch, SIGNAL(execStop(bool)),
          this     , SLOT  (execStop(bool)));
  connect(&m_oBatch, SIGNAL(execRequest(int,int,int,int,int)),
          this     , SLOT  (execRequest(int,int,int,int,int)));
}

//******************************************************************************

BatchProcessor::~BatchProcessor()
{
  stop(true);
  delete ui;
  delete m_poBatchHighlighter;
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
  updateOutputFile();
  if (!logOpen(m_qsOutputFile)) return;

  setControlsEnabled(false);

  int iPeriod = ui->intervalSpinBox->value();
  if (iPeriod > 0)
  {
    m_bStopAfterExecution = false;
    m_timer.start(iPeriod * 1000);
  }

  runBatch();
}

//******************************************************************************

void BatchProcessor::stop(bool bForce)
{
  ui->stopButton->setEnabled(false);

  m_bStopAfterExecution = true;
  m_timer.stop();

  if (m_oBatch.isExecuting())
  {
    m_oBatch.stop(bForce);
  }
  else {
    setControlsEnabled(true);
  }

  logClose();
}

//******************************************************************************

void BatchProcessor::browseOutputFile()
{
  QString qsFilename =
      QFileDialog::getSaveFileName(this,
                                   tr("Get output file"),
                                   QString(),
                                   tr("CSV files (*.csv)"),
                                   NULL,
                                   QFileDialog::DontConfirmOverwrite);
  if (!qsFilename.isEmpty())
  {
    ui->outputFileEdit->setText(qsFilename);
  }
}

//******************************************************************************

void BatchProcessor::updateOutputFile()
{
  QString qsTooltip;

  m_qsOutputFile = ui->outputFileEdit->text();

  if (m_qsOutputFile.isEmpty())
  {
    qsTooltip = "If empty, the output is only logged below.";
  }
  else
  {
    // replace wildcards
    QDateTime qDate = QDateTime::currentDateTime();
    m_qsOutputFile.replace("$DATE", qDate.toString("yyyyMMdd"));
    m_qsOutputFile.replace("$TIME", qDate.toString("hhmmss"));
    m_qsOutputFile.replace("$INPUTDIR", m_oInputDir.absolutePath());

    // convert to absolute path
    m_qsOutputFile = QFileInfo(m_qsOutputFile).absoluteFilePath();
    qsTooltip = m_qsOutputFile;
  }

  // show the result in the tooltip
  ui->outputFileEdit->setToolTip(
    qsTooltip + "<p>See the context help for details (<b>Shift+F1</b>).</p>"
  );
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

void BatchProcessor::saveBatchFile()
{
  QString sFilename = QFileDialog::getSaveFileName(this,
                                                   tr("Save batch file"),
                                                   QString(),
                                                   tr("QModBus Batch (*.qmb)"));
  if (sFilename.isEmpty()) return;

  QFile qFile(sFilename);
  if (!qFile.open(QIODevice::WriteOnly))
  {
    QMessageBox::warning(this,
                         tr("Could not open file"),
                         tr("Could not open batch file %1 for writing.")
                            .arg(sFilename));
    return;
  }

  QTextStream qStream(&qFile);
  qStream << ui->batchEdit->toPlainText();
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
                            .arg(sFilename));
    return false;
  }

  ui->batchEdit->setPlainText(qFile.readAll());
  this->updateBatchFileMenu(QFileInfo(sFilename).absolutePath());

  return true;
}

//******************************************************************************

void BatchProcessor::runBatch()
{
  m_oBatch.exec();
}

//******************************************************************************

bool BatchProcessor::validateBatch()
{
  return m_oBatch.isValid();
}

//******************************************************************************

void BatchProcessor::setControlsEnabled(bool bEnable)
{
  ui->batchEdit->setReadOnly(!bEnable);
  ui->startButton->setEnabled(bEnable);
  ui->stopButton->setEnabled(!bEnable);
  repaint();
}

//******************************************************************************

void BatchProcessor::highlightCommand(int nPos)
{
  QList<QTextEdit::ExtraSelection> qaSelections;
  QTextEdit::ExtraSelection qSelection;

  int nCursorPos;
  QString qsText = ui->batchEdit->toPlainText();
  qSelection.cursor = QTextCursor(ui->batchEdit->document());
  // select from the first non-whitespace character...
  nCursorPos = qsText.indexOf(QRegExp("\\S"), m_oBatch.at(nPos)->start());
  qSelection.cursor.setPosition(nCursorPos);
  // ... to the last non-whitespace character
  nCursorPos = qsText.lastIndexOf(QRegExp("\\S"), m_oBatch.at(nPos)->end());
  qSelection.cursor.setPosition(nCursorPos+1, QTextCursor::KeepAnchor);

  qSelection.format.setBackground(Qt::yellow);

  qaSelections.append(qSelection);
  ui->batchEdit->setExtraSelections(qaSelections);
}

//******************************************************************************

void BatchProcessor::batchChanged()
{
  // @PERIOD
  const Batch::CDirectivePeriod * poPeriod = m_oBatch.period();

  ui->intervalSpinBox->setEnabled(poPeriod == NULL);
  if (poPeriod)
  {
    ui->intervalSpinBox->setValue(poPeriod->period());
  }

  // @OUTPUT
  const Batch::CDirectiveOutput * poOutput = m_oBatch.output();

  ui->outputFileEdit->setEnabled(poOutput == NULL);
  ui->openOutputFileButton->setEnabled(poOutput == NULL);
  if (poOutput)
  {
    ui->outputFileEdit->setText(poOutput->path());
  }
}

//******************************************************************************

void BatchProcessor::execStart()
{
  //
}

//******************************************************************************

void BatchProcessor::execStop(bool bFinished)
{
  // clear selection
  ui->batchEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());

  // re-enable the controls (if fully stopped)
  if (m_bStopAfterExecution) setControlsEnabled(true);
}

//******************************************************************************

void BatchProcessor::execRequest(int            iSlaveId,
                                 int            iFuncId,
                                 int            iAddr,
                                 int            iVal,
                                 int            iNum)
{
  QString qStrCommon =
      QString("%1, %2, 0x%3, ")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
        .arg(iSlaveId)
        .arg(QString::number(iFuncId, 16).toUpper());

  try {
    QVector<uint16_t> qau16Result =
        sendModbusRequest(iSlaveId, iFuncId, iAddr, iVal, iNum);

    foreach (uint16_t u16Val, qau16Result)
    {
      logWrite(qStrCommon + QString::number(iAddr++) + ", " + QString::number(u16Val));
    }

  } catch (const QString & qsErr) {
    logWrite(qStrCommon + QString::number(iAddr) + ", " + qsErr);
  }
}

//******************************************************************************

QVector<uint16_t> BatchProcessor::sendModbusRequest(int iSlaveID,
                                                    int iFuncId,
                                                    int iAddr,
                                                    int iNum,
                                                    int iParam)
{
  if ((m_modbus == NULL) || (iNum < 1))
  {
    return QVector<uint16_t>();
  }

  QVector<uint16_t>   qau16Result(iNum);

  uint16_t * au16Data = qau16Result.data();
  uint8_t  * au8Data  = (uint8_t*)au16Data;
  bool       b8Bit    = false;
  int        ret      = -1;

  modbus_set_slave(m_modbus, iSlaveID);

  switch (iFuncId)
  {
    case MODBUS_FC_READ_COILS:
      ret = modbus_read_bits(m_modbus, iAddr, iNum, au8Data);
      b8Bit = true;
      break;

    case MODBUS_FC_READ_DISCRETE_INPUTS:
      ret = modbus_read_input_bits(m_modbus, iAddr, iNum, au8Data);
      b8Bit = true;
      break;

    case MODBUS_FC_READ_HOLDING_REGISTERS:
      ret = modbus_read_registers(m_modbus, iAddr, iNum, au16Data);
      break;

    case MODBUS_FC_READ_INPUT_REGISTERS:
      ret = modbus_read_input_registers(m_modbus, iAddr, iNum, au16Data);
      break;

    case MODBUS_FC_READ_FILE_RECORD:
      ret = modbus_read_file_record(m_modbus, iParam, iAddr, iNum, au16Data);
      break;

    case MODBUS_FC_WRITE_SINGLE_COIL:
      ret = modbus_write_bit(m_modbus, iAddr, iParam);
      au16Data[0] = iParam;
      break;

    case MODBUS_FC_WRITE_SINGLE_REGISTER:
      ret = modbus_write_register(m_modbus, iAddr, iParam);
      au16Data[0] = iParam;
      break;

    case MODBUS_FC_WRITE_MULTIPLE_COILS:
    {
      for (int i = 0; i < iNum; ++i)
      {
        au8Data[i] = iParam;
      }
      b8Bit = true;
      ret = modbus_write_bits(m_modbus, iAddr, iNum, au8Data);
      break;
    }

    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
    {
      for (int i = 0; i < iNum; ++i)
      {
        au16Data[i] = iParam;
      }
      ret = modbus_write_registers(m_modbus, iAddr, iNum, au16Data);
      break;
    }

    default:
      // should not happen, as we validate the batch prior to execution
      throw tr("-1 (Function code %1 not implemented").arg(iFuncId);
      break;
  }

  if (ret == iNum)
  {
    if (b8Bit)
    {
      // convert the 8bit array to 16bit array (from the back!)
      for (int i = iNum-1; i >= 0; --i)
      {
        au16Data[i] = au8Data[i];
      }
    }

    return qau16Result;
  }

  else if (ret < 0)
  {
    if ((errno == EIO) ||
#ifdef WIN32
        (errno == WSAETIMEDOUT) ||
#endif
        (0))
    {
      throw tr("-1 (I/O error: did not receive any data from slave.)");
    }
    else
    {
      throw tr("-1 (Slave threw exception \"%1\" or function not implemented.)")
                .arg(modbus_strerror(errno));
    }
  }
  else
  {
    throw tr("-1 (Number of registers returned does not match "
             "number of registers requested!)");
  }
}

//******************************************************************************

bool BatchProcessor::logOpen(const QString & sFilename)
{
  ui->txtLog->clear();

  if (sFilename.isEmpty()) return true;

  m_oOutputFile.setFileName(sFilename);

  QIODevice::OpenMode qeMode = QFile::Append;
  if (m_oOutputFile.exists())
  {
    QMessageBox::StandardButton qeResult =
      QMessageBox::question(
        this,
        tr("File exists"),
        tr("File %1 already exists. Overwrite?\n"
           "Press \"Yes\" to overwrite, \"No\" to append.")
            .arg(m_oOutputFile.fileName()),
        (QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel),
        QMessageBox::No
      );
    if (QMessageBox::Cancel == qeResult) return false;
    if (QMessageBox::Yes    == qeResult) qeMode = QFile::Truncate;
  }

  if(!m_oOutputFile.open(QFile::WriteOnly | qeMode))
  {
    QMessageBox::critical(this,
                          tr("Could not open file"),
                          tr("Could not open output file %1 for writing.")
                            .arg(m_oOutputFile.fileName()));
    return false;
  }

  ui->txtLog->appendPlainText("Logging to " + m_oOutputFile.fileName() + "\n");
  return true;
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
