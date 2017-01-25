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
public:
  int   m_nDuration;

  CDelay(const QString & qsCommand, int nStart);

  virtual ECommandType type() const { return neCommandDelay; }
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
};

//****************************************************************************//

class CBatch
{
protected:
  QList<CCommand *> m_qapoCommands;
  QString           m_qsBatch;

public:
  CBatch(const QString & qsBatch);
  virtual ~CBatch();

  void rebuild(const QString & qsBatch);

  int count(void) const { return m_qapoCommands.count(); }
  bool isValid(void) const;
  CCommand * at(int nPos) const;
  int commandIndex(int nCharPos) const;

private:
  void free(void);
  void create(const QString & qsBatch);
};

//****************************************************************************//

}

//****************************************************************************//

#endif // _BATCH_PARSER_H //
