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
#define SEPARATOR_REQ_DATA_RANGE        '-'
#define SEPARATOR_DIR_NAME_DATA         '='

#define REQ_SLAVE_ID_MIN                (0)
#define REQ_SLAVE_ID_MAX                (254)
#define REQ_ADDR_MIN                    (0)
#define REQ_ADDR_MAX                    (65535)
#define REQ_VAL_MIN                     (0)
#define REQ_VAL_MAX                     (65535)

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

int CCommand::validateInt(const QString & qsStr, int nBase, int nMin, int nMax)
{
  bool bSucc = true;
  int  ret;

  ret = qsStr.toInt(&bSucc, nBase);

  bSucc &= ((ret >= nMin) && (ret <= nMax));

  m_bValid &= bSucc;
  return ret;
}

//******************************************************************************

bool CCommand::validateTrue(bool bStatement)
{
  m_bValid &= bStatement;
  return bStatement;
}

//******************************************************************************

QString CCommand::noWhitespace(const QString & qsStr)
{
  QString qsWorkStr(qsStr);
  return qsWorkStr.remove(QRegExp("\\s"));
}

//******************************************************************************

QString CCommand::skipChar(const QString & qsStr, char c)
{
  int nPos = qsStr.indexOf(c) + 1;
  // if the char was not found, we get (-1)+(1)=0 -> full string
  return qsStr.mid(nPos);
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
  // nothing to do
}

//******************************************************************************

CDirective * CDirective::parse(const QString& qsCommand, int nStart)
{
  const QString qsCommandSkipped = skipChar(qsCommand, STARTCHAR_DIRECTIVE);
  const QString qsName = qsCommandSkipped.section(SEPARATOR_DIR_NAME_DATA, 0, 0)
                          .trimmed();
  const QString qsData = qsCommandSkipped.section(SEPARATOR_DIR_NAME_DATA, 1);

  if ("PERIOD" == qsName)
  {
    return new CDirectivePeriod(qsCommand, nStart, qsData);
  }
  else if ("OUTPUT" == qsName)
  {
    return new CDirectiveOutput(qsCommand, nStart, qsData);
  }
  else
  {
    return new CDirectiveInvalid(qsCommand, nStart);
  }
}

//******************************************************************************

CDirectiveInvalid::CDirectiveInvalid(const QString & qsCommand, int nStart):
  CDirective(qsCommand, nStart)
{
  validateTrue(false);
}

//******************************************************************************

CDirectivePeriod::CDirectivePeriod(const QString & qsCommand, int nStart,
                                   const QString & qsData):
  CDirective(qsCommand, nStart)
{
  m_nPeriod = validateInt(qsData, 10, 0);
}

//******************************************************************************

CDirectiveOutput::CDirectiveOutput(const QString & qsCommand, int nStart,
                                   const QString & qsData):
  CDirective(qsCommand, nStart)
{
  m_qsPath = qsData.trimmed();
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CDelay::CDelay(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart)
{
  m_nDuration = validateInt(skipChar(qsCommand, STARTCHAR_DELAY), 10, 1);
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CRequest::CRequest(const QString & qsCommand, int nStart):
  CCommand(qsCommand, nStart),
  m_nSlaveId(-1),
  m_nFuncId(-1)
{
  const QStringList qasCommand = qsCommand.split(SEPARATOR_REQ_FUNC_DATA);
  if (!validateTrue(2 == qasCommand.length())) return;

  // parse the destination (slave, func)
  const QStringList qasDst = qasCommand.first().split(SEPARATOR_REQ_SLAVE_FUNC);
  if (!validateTrue(2 == qasCommand.length())) return;

  m_nSlaveId  = validateInt(qasDst.first(), 10,
                            REQ_SLAVE_ID_MIN, REQ_SLAVE_ID_MAX);
  m_nFuncId   = validateInt(qasDst.last (), 16);

  TFuncType oFuncType = CRequest::getFuncType(m_nFuncId);
  validateTrue(neFuncOperationInvalid != oFuncType.eOperation);
  if (!valid()) return;

  // parse the data
  const QStringList qasData = qasCommand.last().split(SEPARATOR_REQ_DATA_DATA);
  if (!validateTrue(qasData.length() > 0)) return;

  foreach (const QString & qsData, qasData)
  {
    const QStringList qasAddrVal = qsData.split(SEPARATOR_REQ_DATA_VAL);

    int nVal = 0;
    switch (oFuncType.eOperation)
    {
      case neFuncOperationRead:
        validateTrue(qasAddrVal.count() == 1);                          // ADDRS
        break;

      case neFuncOperationWrite:
        validateTrue(qasAddrVal.count() == 2);                          // ADDRS=VAL
        nVal = validateInt(qasAddrVal.last(), 10,
                           REQ_VAL_MIN, REQ_VAL_MAX);
        break;

      default:
        // should not happen
        validateTrue(false);
        break;
    }
    if (!valid()) return;

    int nAddr1=0, nAddr2=0;
    const QStringList qasAddrs = qasAddrVal.first().split(SEPARATOR_REQ_DATA_RANGE);
    switch (qasAddrs.length())
    {
      case 1:                                                           // ADDR
        nAddr1 = validateInt(qasAddrs.first(), 10,
                             REQ_ADDR_MIN, REQ_ADDR_MAX);
        nAddr2 = nAddr1;
        break;

      case 2:                                                           // ADDR1-ADDR2
        nAddr1 = validateInt(qasAddrs.first(), 10,
                             REQ_ADDR_MIN, REQ_ADDR_MAX);
        nAddr2 = validateInt(qasAddrs.last(), 10,
                             REQ_ADDR_MIN, REQ_ADDR_MAX);
        validateTrue(nAddr1 < nAddr2);
        break;

      default:
        validateTrue(false);
        break;
    }

    for (int nAddr=nAddr1; nAddr<=nAddr2; ++nAddr)
    {
      m_qanAddrs.append(nAddr);
      m_qanVals .append(nVal );
    }
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
  m_poProcessor = new CBatchProc(this);
  this->create(qsBatch);
}

//******************************************************************************

CBatch::~CBatch()
{
  this->free();
  delete m_poProcessor;
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
          poCommand = CDirective::parse(qsCommand, nPos);
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
  emit changed();
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

const CCommand * CBatch::at(int nPos) const
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

void CBatch::exec(void) const
{
  m_poProcessor->start();
}

//******************************************************************************

void CBatch::stop(bool bForce)
{
  m_poProcessor->stop();
  if (bForce)
  {
    m_poProcessor->terminate();
    m_poProcessor->wait();
  }
}

//******************************************************************************

bool CBatch::isExecuting(void) const
{
  return m_poProcessor->isRunning();
}

//******************************************************************************

const CDirectivePeriod * CBatch::period() const
{
  const CDirectivePeriod * poDirective = NULL;

  for (int i=0; i<m_qapoCommands.count(); ++i)
  {
    poDirective = dynamic_cast<const CDirectivePeriod *>(m_qapoCommands.at(i));
    if (poDirective) return poDirective;
  }

  return NULL;
}

//******************************************************************************

const CDirectiveOutput * CBatch::output() const
{
  const CDirectiveOutput * poDirective = NULL;

  for (int i=0; i<m_qapoCommands.count(); ++i)
  {
    poDirective = dynamic_cast<const CDirectiveOutput *>(m_qapoCommands.at(i));
    if (poDirective) return poDirective;
  }

  return NULL;
}

//******************************************************************************
//******************************************************************************
//******************************************************************************

CBatchProc::CBatchProc(const CBatch * poBatch):
  QThread(),
  m_poBatch(poBatch),
  m_bStop(false)
{
  //
}

//******************************************************************************

void CBatchProc::run()
{
  m_bStop = false;
  emit m_poBatch->execStart();

  for (int i=0; i<m_poBatch->m_qapoCommands.count() && !m_bStop; ++i)
  {
    CCommand * poCommand = m_poBatch->m_qapoCommands.at(i);
    switch (poCommand->type())
    {
      case neCommandDelay:
        emit m_poBatch->execCommand(i);

        this->msleep(((CDelay*)poCommand)->duration());
        break;

      case neCommandRequest:
      {
        emit m_poBatch->execCommand(i);
        this->msleep(10); // give GUI some time to process the signal

        CRequest * poRequest = (CRequest*)poCommand;
        for (int j=0; j<poRequest->addrs().count(); ++j)
        {
          emit m_poBatch->execRequest(poRequest->slaveId(),
                                      poRequest->funcId(),
                                      poRequest->addrs()[j],
                                      poRequest->vals()[j]);
        }
        break;
      }

      default:
        // nothing to execute
        break;
    }
  }

  emit m_poBatch->execStop(!m_bStop);
}

//******************************************************************************

void CBatchProc::stop()
{
  this->m_bStop = true;
}

//******************************************************************************
