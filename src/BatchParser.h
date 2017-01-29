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

#include <climits>

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
  static QString skipChar(const QString & qsStr, char c);

public:
  CCommand(const QString & qsCommand, int nStart);
  virtual ~CCommand();

  virtual ECommandType type() const = 0;

  int start() const { return m_nTextStart; }
  int len()   const { return m_nTextLen; }
  int end()   const { return m_nTextStart + m_nTextLen - 1; }

  bool valid() const { return m_bValid; }

  /** Converts string to int and checks its range, updating the m_bValid state. */
  int validateInt(const QString       & qsStr,
                  int                   nBase = 10,
                  int                   nMin = INT_MIN,
                  int                   nMax = INT_MAX);
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
protected:
  CDirective(const QString & qsCommand, int nStart);

public:
  virtual ECommandType type() const { return neCommandDirective; }

  /** Creates a directive object of respective subtype. */
  static CDirective * parse(const QString & qsCommand, int nStart);
};

class CDirectiveInvalid : public CDirective
{
public:
  CDirectiveInvalid(const QString & qsCommand, int nStart);
};

class CDirectivePeriod : public CDirective
{
protected:
  int           m_nPeriod;

public:
  int period() const { return m_nPeriod; }

  CDirectivePeriod(const QString & qsCommand, int nStart,
                   const QString & qsData);
};

class CDirectiveOutput : public CDirective
{
protected:
  QString       m_qsPath;

public:
  const QString & path() const { return m_qsPath; }

  CDirectiveOutput(const QString & qsCommand, int nStart,
                   const QString & qsData);
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

  /** Retrieves the number of all commands in the batch. */
  int count(void) const { return m_qapoCommands.count(); }
  /** Checks for validity of all batch commands. */
  bool isValid(void) const;
  /** Retrieves pointer to the command at given position, NULL on failure. */
  const CCommand * at(int nPos) const;
  /** Retrieves index of command at given character position in the text,
   *  or the last command preceding given character position. */
  int commandIndex(int nCharPos) const;

  /** Retrieves the first PERIOD directive pointer or NULL. */
  const CDirectivePeriod * period() const;
  /** Retrieves the first OUTPUT directive pointer or NULL. */
  const CDirectiveOutput * output() const;

signals:
  /** Emitted when the batch model changed. */
  void changed() const;

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
