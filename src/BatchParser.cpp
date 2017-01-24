/*
 * BatchParser.cpp - header file for BatchParser class
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

#include "BatchParser.h"
#include <QRegExp>
#include <QStringList>

using namespace Batch;

//******************************************************************************

#define COMMAND_SEPARATOR               ';'
#define STARTCHAR_DELAY                 '+'
#define STARTCHAR_DIRECTIVE             '@'
#define STARTCHAR_COMMENT               '#'

#define REQUEST_ADDR_SEPARATOR          ':'
#define REQUEST_DATA_SEPARATOR          ','

//******************************************************************************
//******************************************************************************
//******************************************************************************

CCommand::CCommand(const QString & qsCommand, int nStart):
  m_nTextStart(nStart),
  m_bValid(true)
{
  m_nTextLen = qsCommand.length();
}

//******************************************************************************

CCommand::~CCommand()
{
  //
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CEmpty::CEmpty(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  //
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CComment::CComment(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  //
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CDirective::CDirective(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  //
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CDelay::CDelay(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  //
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CRequest::CRequest(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  //
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CBatch::CBatch(const QString & qsBatch)
{
  this->create(qsBatch);
}

//******************************************************************************

CBatch::~CBatch()
{
  this->free();
}

//******************************************************************************

void CBatch::rebuild(const QString & qsBatch)
{
  if (m_qsBatch != qsBatch)
  {
    this->free();
    this->create(qsBatch);
  }
}

//******************************************************************************

void CBatch::create(const QString & qsBatch)
{
  const QStringList   qasCommands = qsBatch.split(COMMAND_SEPARATOR);
  int                 nPos = 0;
  QRegExp             qNonWhitespace("\\S");
  CCommand          * poCommand;

  foreach (const QString & qsCommand, qasCommands)
  {
    int nCmdStart = qsCommand.indexOf(qNonWhitespace);

    if (-1 == nCmdStart)
    {
      poCommand = new CEmpty(qsCommand, nPos);
    }
    else
    {
      switch (qsCommand.at(nCmdStart).toAscii())
      {
        case STARTCHAR_DELAY:
          poCommand = new CDelay(qsCommand, nPos);
          break;

        case STARTCHAR_DIRECTIVE:
          poCommand = new CDirective(qsCommand, nPos);
          break;

        case STARTCHAR_COMMENT:
          poCommand = new CComment(qsCommand, nPos);
          break;

        default:
          poCommand = new CRequest(qsCommand, nPos);
          break;
      }
    }

    m_qapoCommands.append(poCommand);
    nPos = poCommand->end() + 2;  // skip the separator
  }

  m_qsBatch = qsBatch;
}

//******************************************************************************

void CBatch::free(void)
{
  foreach (CCommand * poCommand, m_qapoCommands)
  {
    delete poCommand;
  }
  m_qapoCommands.clear();
}

//******************************************************************************

bool CBatch::isValid(void)
{
  foreach (CCommand * poCommand, m_qapoCommands)
  {
    if (!poCommand->valid()) return false;
  }
  return true;
}

//******************************************************************************

CCommand * CBatch::at(int nPos)
{
  if ((nPos < 0) || (nPos >= m_qapoCommands.count())) return NULL;
  return m_qapoCommands.at(nPos);
}

//******************************************************************************

int CBatch::commandIndex(int nCharPos)
{
  int i;

  for (i=0; i<m_qapoCommands.count(); ++i)
  {
    if (m_qapoCommands.at(i)->start() > nCharPos) break;
  }
  return i-1;
}

//******************************************************************************
