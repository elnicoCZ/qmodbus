/*
 * BatchParser.h - header file for BatchParser class
 *
 * Copyright (c) 2017      Petr Kubiznak
 *
 * This file is part of QModBus - https://github.com/elnicoCZ/qmodbus
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

#ifndef _BATCH_PARSER_H
#define _BATCH_PARSER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QList>

//****************************************************************************//

namespace Batch {

//****************************************************************************//

typedef enum {
  neCommandEmpty,
  neCommandComment,
  neCommandDirective,
  neCommandDelay,
  neCommandRequest,

} ECommandType;

class CBatchProc;

//****************************************************************************//

class CCommand
{
protected:
  int   m_nTextStart;
  int   m_nTextLen;
  bool  m_bValid;

  static QString noWhitespace(const QString & qsStr);

public:
  CCommand(const QString & qsCommand, int nStart);
  virtual ~CCommand();

  virtual ECommandType type() const = 0;

  int start() const { return m_nTextStart; }
  int len()   const { return m_nTextLen; }
  int end()   const { return m_nTextStart + m_nTextLen - 1; }

  bool valid() const { return m_bValid; }

  /** Converts string to int and checks its range, updating the m_bValid state. */
  int validateInt(const QString qStr, int nBase=10, int nMin=0, int nMax=-1);
  /** Asserts given statement is true, updating the m_bValid state. */
  bool validateTrue(bool bStatement);
};

//****************************************************************************//

class CEmpty : public CCommand
{
public:
  CEmpty(const QString & qsCommand, int nStart);

  virtual ECommandType type() const { return neCommandEmpty; }
};

//****************************************************************************//

class CComment : public CCommand
{
public:
  CComment(const QString & qsCommand, int nStart);

  virtual ECommandType type() const { return neCommandComment; }
};

//****************************************************************************//

class CDirective : public CCommand
{
public:
  CDirective(const QString & qsCommand, int nStart);

  virtual ECommandType type() const { return neCommandDirective; }
};

//****************************************************************************//

class CDelay : public CCommand
{
protected:
  int   m_nDuration;

public:
  CDelay(const QString & qsCommand, int nStart);

  virtual ECommandType type() const { return neCommandDelay; }

  int duration() const { return m_nDuration; }
};

//****************************************************************************//

class CRequest : public CCommand
{
protected:
  typedef enum EFuncOperation_
  {
    neFuncOperationInvalid,
    neFuncOperationRead,
    neFuncOperationWrite
  } EFuncOperation;

  typedef enum EFuncSubject_
  {
    neFuncSubjectInvalid,
    neFuncSubjectCoil,
    neFuncSubjectHoldingReg,
    neFuncSubjectInputReg,
    neFuncSubjectDiscreteInput
  } EFuncSubject;

  typedef enum EFuncScope_
  {
    neFuncScopeInvalid,
    neFuncScopeSingle,
    neFuncScopeMultiple
  } EFuncScope;

  typedef struct TFuncType_
  {
    EFuncOperation  eOperation;
    EFuncSubject    eSubject;
    EFuncScope      eScope;
  } TFuncType;

  static TFuncType getFuncType(int nFuncId);

protected:
  int         m_nSlaveId;
  int         m_nFuncId;
  QList<int>  m_qanAddrs;
  QList<int>  m_qanVals;

public:
  CRequest(const QString & qsCommand, int nStart);

  virtual ECommandType type() const { return neCommandRequest; }

  int slaveId() const { return m_nSlaveId; }
  int funcId () const { return m_nFuncId ; }
  const QList<int> & addrs() const { return m_qanAddrs; }
  const QList<int> & vals () const { return m_qanVals ; }
};

//****************************************************************************//

class CBatch: public QObject
{
  Q_OBJECT

  friend class CBatchProc;

protected:
  QList<CCommand *>   m_qapoCommands;
  QString             m_qsBatch;
  CBatchProc        * m_poProcessor;

public:
  CBatch(const QString & qsBatch);
  virtual ~CBatch();

  /** Rebuilds the batch model if the string differs from the current batch. */
  void rebuild(const QString & qsBatch);
  /** Executes the batch. */
  void exec(void) const;
  /** Initiates stopping the execution.
   * @param[in] bForce  Use true to terminate the execution immediately.
   *                    This might have negative impact on open resources! */
  void stop(bool bForce = false);
  /** Checkes whether the batch is currently being executed. */
  bool isExecuting(void) const;

  int count(void) const { return m_qapoCommands.count(); }
  bool isValid(void) const;
  CCommand * at(int nPos) const;
  int commandIndex(int nCharPos) const;

signals:
  /** Execution started. */
  void execStart() const;
  /** Execution complete. */
  void execStop(bool bFinished) const;
  /** Executing command at given index. */
  void execCommand(int nPos) const;

  /** Request command handler. */
  void execRequest(int nSlaveId, int nFuncId, int nAddr, int nVal) const;

private:
  void free(void);
  void create(const QString & qsBatch);
};

//****************************************************************************//

class CBatchProc: public QThread
{
protected:
  const CBatch  * m_poBatch;
  bool            m_bStop;

  virtual void run();

public:
  CBatchProc(const CBatch * poBatch);

  /** Stop execution. */
  void stop();
};

//****************************************************************************//

}

//****************************************************************************//

#endif // _BATCH_PARSER_H //
