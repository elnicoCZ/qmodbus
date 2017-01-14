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
  m_timer()
{
  ui->setupUi(this);

  connect(&m_timer, SIGNAL(timeout()), this, SLOT(runBatch()));
}

//******************************************************************************

BatchProcessor::~BatchProcessor()
{
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

  m_outputFile.close();

  m_outputFile.setFileName( ui->outputFileEdit->text() );
  if( !m_outputFile.open( QFile::WriteOnly | QFile::Truncate ) )
  {
    QMessageBox::critical(this,
                          tr("Could not open file"),
                          tr("Could not open output file %1 for writing.")
                            .arg(m_outputFile.fileName()));
    return;
  }

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
  m_outputFile.close();

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

void BatchProcessor::runBatch()
{
	processBatch(ui->slaveEdit->text(), true);
}

//******************************************************************************

bool BatchProcessor::validateBatch()
{
	return processBatch(ui->slaveEdit->text(), false);
}

//******************************************************************************

bool BatchProcessor::processBatch(const QString & qBatch, bool bExecute)
{
  bool              bParseSucc = true;
  const QStringList qSlaves    = qBatch.split( ';' );

  foreach( const QString & qSlave, qSlaves )
  {
    const QString     qCommand = qSlave.split('!').first();
    const QStringList qDatas   = qSlave.split('!').last ().split(',');
    bool              bSucc    = true;

    const int         iSlaveId = qCommand.split('x').first().toInt(&bSucc);     bParseSucc &= bSucc;
    const int         iFuncId  = qCommand.split('x').last ().toInt(&bSucc, 16); bParseSucc &= bSucc;

    foreach( const QString & qData, qDatas )
    {
      const QStringList qAddrVal = qData.split(':');
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
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
      return neFuncTypeRead;

    case 0x0F:
    case 0x10:
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
  QTextStream out(&m_outputFile);

  out << QDateTime::currentDateTime().toString() << ", "
      << iSlaveId << ", "
      << "0x" << QString::number(iFuncId, 16).toUpper() << ", "
      << iAddr << ", "
      << sendModbusRequest(iSlaveId, iFuncId, iAddr)
      << endl;
}

//******************************************************************************

QString BatchProcessor::sendModbusRequest( int slaveID, int func, int addr )
{
	if( m_modbus == NULL )
	{
		return QString();
	}

	const int num = 1;
	uint8_t dest[num*sizeof(uint16_t)];
	uint16_t * dest16 = (uint16_t *) dest;

	memset( dest, 0, sizeof( dest ) );

	int ret = -1;
	bool is16Bit = false;

	modbus_set_slave( m_modbus, slaveID );

	switch( func )
	{
		case MODBUS_FC_READ_COILS:
			ret = modbus_read_bits( m_modbus, addr, num, dest );
			break;
		case MODBUS_FC_READ_DISCRETE_INPUTS:
			ret = modbus_read_input_bits( m_modbus, addr, num, dest );
			break;
		case MODBUS_FC_READ_HOLDING_REGISTERS:
			ret = modbus_read_registers( m_modbus, addr, num, dest16 );
			is16Bit = true;
			break;
		case MODBUS_FC_READ_INPUT_REGISTERS:
			ret = modbus_read_input_registers( m_modbus, addr, num, dest16 );
			is16Bit = true;
			break;
/*		case MODBUS_FC_WRITE_SINGLE_COIL:
			ret = modbus_write_bit( m_modbus, addr,
					ui->regTable->item( 0, DataColumn )->
						text().toInt(0, 0) ? 1 : 0 );
			writeAccess = true;
			num = 1;
			break;
		case MODBUS_FC_WRITE_SINGLE_REGISTER:
			ret = modbus_write_register( m_modbus, addr,
					ui->regTable->item( 0, DataColumn )->
						text().toInt(0, 0) );
			writeAccess = true;
			num = 1;
			break;

		case MODBUS_FC_WRITE_MULTIPLE_COILS:
		{
			uint8_t * data = new uint8_t[num];
			for( int i = 0; i < num; ++i )
			{
				data[i] = ui->regTable->item( i, DataColumn )->
								text().toInt(0, 0);
			}
			ret = modbus_write_bits( m_modbus, addr, num, data );
			delete[] data;
			writeAccess = true;
			break;
		}
		case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
		{
			uint16_t * data = new uint16_t[num];
			for( int i = 0; i < num; ++i )
			{
				data[i] = ui->regTable->item( i, DataColumn )->
								text().toInt(0, 0);
			}
			ret = modbus_write_registers( m_modbus, addr, num, data );
			delete[] data;
			writeAccess = true;
			break;
		}*/

		default:
			QMessageBox::warning( this, tr( "Unimplemented function code" ), tr( "Function code %1 not implemented" ).arg( func ) );
			break;
	}

	if( ret == num )
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
		if( ret < 0 )
		{
			if(
#ifdef WIN32
					errno == WSAETIMEDOUT ||
#endif
					errno == EIO
																	)
			{
				QMessageBox::critical( this, tr( "I/O error" ),
					tr( "I/O error: did not receive any data from slave." ) );
			}
			else
			{
				QMessageBox::critical( this, tr( "Protocol error" ),
					tr( "Slave threw exception \"%1\" or "
						"function not implemented." ).
								arg( modbus_strerror( errno ) ) );
			}
		}
		else
		{
			QMessageBox::critical( this, tr( "Protocol error" ),
				tr( "Number of registers returned does not "
					"match number of registers "
							"requested!" ) );
		}
	}

	return "-1 (NO VALID DATA RECEIVED)";
}
