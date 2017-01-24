/*
 * BatchProcessor.h - header file for BatchProcessor class
 *
 * Copyright (c) 2011-2014 Tobias Doerffel / Electronic Design Chemnitz
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

#ifndef _BATCH_PROCESSOR_H
#define _BATCH_PROCESSOR_H

#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDialog>
#include <QMenu>
#include <QTextDocument>
#include <QSyntaxHighlighter>

#include "BatchParser.h"
#include "modbus.h"

namespace Ui
{
  class BatchProcessor;
} ;

class BatchHighlighter : public QSyntaxHighlighter
{
public:
  BatchHighlighter(QTextDocument * parent, Batch::CBatch & oBatch);

protected:
  Batch::CBatch   & m_oBatch;

  virtual void highlightBlock(const QString & text);
};

class BatchProcessor : public QDialog
{
  Q_OBJECT
public:
  BatchProcessor( QWidget *parent, modbus_t *modbus );
  ~BatchProcessor();

private:
  typedef enum EFuncType_
  {
    neFuncTypeRead,
    neFuncTypeWrite,
    neFuncTypeInvalid
  } EFuncType;

  /** */
  bool validateBatch();
  /** */
  bool processBatch(const QString & qBatch, bool bExecute);
  /** */
  void execRequest(int            iSlaveId,
                   int            iFuncId,
                   int            iAddr,
                   int            iVal);
  /** */
  static EFuncType getFuncType(int iFuncId);

  /** */
  void logOpen(const QString & sFilename);
  /** */
  void logWrite(const QString & qStr);
  /** */
  void logClose(void);

  /** */
  bool loadBatchFile(const QString & sFilename);
  /** */
  void updateBatchFileMenu(const QString & sPath = "");

private slots:
  void start();
  void stop();
  void browseOutputFile();
  void browseBatchFile();
  void batchMenuTriggered(QAction * qAction);
  void runBatch();

private:
  QString sendModbusRequest(int iSlaveID,
                            int iFuncId,
                            int iAddr,
                            int iVal);

  Ui::BatchProcessor *ui;
  modbus_t *m_modbus;
  QTimer m_timer;
  QDir  m_oInputDir;
  QMenu m_oInputMenu;
  QFile m_oOutputFile;
  BatchHighlighter * m_poBatchHighlighter;
  Batch::CBatch m_oBatch;

} ;





#endif // MAINWINDOW_H
