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

public:
  CCommand(const QString & qsCommand, int nStart);
  virtual ~CCommand();

  virtual ECommandType type() = 0;

  int start() { return m_nTextStart; }
  int len()   { return m_nTextLen; }
  int end()   { return m_nTextStart + m_nTextLen - 1; }

  bool valid() { return m_bValid; }
};

//****************************************************************************//

class CEmpty : public CCommand
{
public:
  CEmpty(const QString & qsCommand, int nStart);

  virtual ECommandType type() { return neCommandEmpty; }
};

//****************************************************************************//

class CComment : public CCommand
{
public:
  CComment(const QString & qsCommand, int nStart);

  virtual ECommandType type() { return neCommandComment; }
};

//****************************************************************************//

class CDirective : public CCommand
{
public:
  CDirective(const QString & qsCommand, int nStart);

  virtual ECommandType type() { return neCommandDirective; }
};

//****************************************************************************//

class CDelay : public CCommand
{
public:
  int   m_nDuration;

  CDelay(const QString & qsCommand, int nStart);

  virtual ECommandType type() { return neCommandDelay; }
};

//****************************************************************************//

class CRequest : public CCommand
{
public:
  int   m_nSlaveId;
  int   m_nFuncId;

  CRequest(const QString & qsCommand, int nStart);

  virtual ECommandType type() { return neCommandRequest; }
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

  int count(void) { return m_qapoCommands.count(); }
  bool isValid(void);
  CCommand * at(int nPos);
  int commandIndex(int nCharPos);

private:
  void free(void);
  void create(const QString & qsBatch);
};

//****************************************************************************//

}

//****************************************************************************//

#endif // _BATCH_PARSER_H //
