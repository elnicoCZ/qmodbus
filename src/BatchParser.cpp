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
#include "modbus-private.h"
#include <QRegExp>
#include <QStringList>

using namespace Batch;

//******************************************************************************

#define STARTCHAR_DELAY                 '+'
#define STARTCHAR_DIRECTIVE             '@'
#define STARTCHAR_COMMENT               '#'

#define SEPARATOR_COMMAND               ';'
#define SEPARATOR_REQ_SLAVE_FUNC        'x'
#define SEPARATOR_REQ_FUNC_DATA         ':'
#define SEPARATOR_REQ_DATA_DATA         ','
#define SEPARATOR_REQ_DATA_VAL          '='

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

QString CCommand::noWhitespace(const QString & qsStr)
{
  QString qsWorkStr(qsStr);
  return qsWorkStr.remove(QRegExp("\\s"));
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CEmpty::CEmpty(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  // empty command is always valid
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CComment::CComment(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  // comment is always valid
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
  m_nDuration = qsCommand.toInt(&m_bValid);
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CRequest::CRequest(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart),
  m_nSlaveId(-1),
  m_nFuncId(-1)
{
  bool bSucc = true;

  const QStringList qasCommand = qsCommand.split(SEPARATOR_REQ_FUNC_DATA);
  m_bValid &= (2 == qasCommand.length()); if (!m_bValid) return;

  // parse the destination (slave, func)
  const QStringList qasDst = qasCommand.first().split(SEPARATOR_REQ_SLAVE_FUNC);
  m_bValid &= (2 == qasCommand.length()); if (!m_bValid) return;

  m_nSlaveId  = qasDst.first().toInt(&bSucc);     m_bValid &= bSucc;
  m_nFuncId   = qasDst.last ().toInt(&bSucc, 16); m_bValid &= bSucc;

  TFuncType oFuncType = CRequest::getFuncType(m_nFuncId);
  m_bValid &= (neFuncOperationInvalid != oFuncType.eOperation);
  if (!m_bValid) return;

  // parse the data
  const QStringList qasData = qasCommand.last().split(SEPARATOR_REQ_DATA_DATA);
  m_bValid &= (qasData.length() > 0); if (!m_bValid) return;

  foreach (const QString & qsData, qasData)
  {
    const QStringList qasAddrVal = qsData.split(SEPARATOR_REQ_DATA_VAL);

    switch (oFuncType.eOperation)
    {
      case neFuncOperationRead:
        m_bValid &= (qasAddrVal.count() == 1);
        break;

      case neFuncOperationWrite:
        m_bValid &= (qasAddrVal.count() == 2);
        break;

      default:
        // should not happen
        m_bValid = false;
        break;
    }
    if (!m_bValid) return;

    m_qanAddrs.append(qasAddrVal.first().toInt(&bSucc)); m_bValid &= bSucc;
    m_qanVals .append(qasAddrVal.last ().toInt(&bSucc)); m_bValid &= bSucc;
    if (!m_bValid) return;
  }

}

//******************************************************************************

CRequest::TFuncType CRequest::getFuncType(int nFuncId)
{
  TFuncType oType =
  {
    .eOperation = neFuncOperationInvalid,
    .eSubject   = neFuncSubjectInvalid,
    .eScope     = neFuncScopeInvalid
  };

  switch (nFuncId)
  {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS:
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS:
      oType.eOperation = neFuncOperationRead;
      break;

    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_SINGLE_REGISTER:
    case MODBUS_FC_WRITE_MULTIPLE_COILS:
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
      oType.eOperation = neFuncOperationWrite;
      break;

    default:
      // return invalid type
      return oType;
  }

  switch (nFuncId)
  {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_MULTIPLE_COILS:
      oType.eSubject = neFuncSubjectCoil;
      break;

    case MODBUS_FC_READ_DISCRETE_INPUTS:
      oType.eSubject = neFuncSubjectDiscreteInput;
      break;

    case MODBUS_FC_READ_INPUT_REGISTERS:
      oType.eSubject = neFuncSubjectInputReg;
      break;

    default:
      oType.eSubject = neFuncSubjectHoldingReg;
      break;
  }

  switch (nFuncId)
  {
    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_SINGLE_REGISTER:
      oType.eScope = neFuncScopeSingle;
      break;

    default:
      oType.eScope = neFuncScopeMultiple;
      break;
  }

  return oType;
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
  const QStringList   qasCommands = qsBatch.split(SEPARATOR_COMMAND);
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

bool CBatch::isValid(void) const
{
  foreach (CCommand * poCommand, m_qapoCommands)
  {
    if (!poCommand->valid()) return false;
  }
  return true;
}

//******************************************************************************

CCommand * CBatch::at(int nPos) const
{
  if ((nPos < 0) || (nPos >= m_qapoCommands.count())) return NULL;
  return m_qapoCommands.at(nPos);
}

//******************************************************************************

int CBatch::commandIndex(int nCharPos) const
{
  int i;

  for (i=0; i<m_qapoCommands.count(); ++i)
  {
    if (m_qapoCommands.at(i)->start() > nCharPos) break;
  }
  return i-1;
}

//******************************************************************************
