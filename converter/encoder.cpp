/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Flacon - audio File Encoder
 * https://github.com/flacon/flacon
 *
 * Copyright: 2012-2013
 *   Alexander Sokoloff <sokoloff.a@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * END_COMMON_COPYRIGHT_HEADER */


#include "encoder.h"
#include "outformat.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>



/************************************************
 *
 ************************************************/
Encoder::Encoder(const WorkerRequest request, const ConverterEnv &env, QObject *parent):
    Worker(parent),
    mRequest(request),
    mEnv(env),
    mTotal(0),
    mReady(0),
    mProgress(0)
{
    mOutFile = mRequest.track()->resultFilePath();
}


/************************************************

 ************************************************/
Encoder::~Encoder()
{

}


/************************************************

 ************************************************/
void Encoder::run()
{
    bool debug  = QProcessEnvironment::systemEnvironment().contains("FLACON_DEBUG_ENCODER");

    {
        QFile f(outFile());

        if (f.exists() && !f.remove())
        {
            error(mRequest.track(), tr("I can't delete file:\n%1\n%2").arg(f.fileName()).arg(f.errorString()));
            return;
        }
    }

    // Input file already WAV, so for WAV output format we just rename file.
    if (mEnv.format->id() == "WAV")
    {
        runWav();
        return;
    }

    QProcess process;
    QStringList args = mEnv.format->encoderArgs(mRequest.track(), QDir::toNativeSeparators(outFile()));
    QString prog = args.takeFirst();

    if (debug)
        debugArguments(prog, args);

    connect(&process, SIGNAL(bytesWritten(qint64)),
            this, SLOT(processBytesWritten(qint64)));

    process.start(QDir::toNativeSeparators(prog), args);
    process.waitForStarted();

    readInputFile(&process);
    process.closeWriteChannel();

    process.waitForFinished(-1);
    if (process.exitCode() != 0)
    {
        debugArguments(prog, args);
        QString msg = tr("QQEncoder error:\n") +
                QString::fromLocal8Bit(process.readAllStandardError());
        error(mRequest.track(), msg);
    }

    deleteFile(mRequest.fileName());

    emit trackReady(mRequest.track(), mOutFile);
}


/************************************************

 ************************************************/
void Encoder::processBytesWritten(qint64 bytes)
{
    mReady += bytes;
    int p = ((mReady * 100.0) / mTotal);
    if (p != mProgress)
    {
        mProgress = p;
        emit trackProgress(mRequest.track(), Track::Encoding, mProgress);
    }
}


/************************************************

 ************************************************/
void Encoder::readInputFile(QProcess *process)
{
    QFile file(mRequest.fileName());
    if (!file.open(QFile::ReadOnly))
    {
        error(mRequest.track(), tr("I can't read %1 file", "Encoder error. %1 is a file name.").arg(mRequest.fileName()));
    }

    mProgress = -1;
    mTotal = file.size();

    int bufSize = int(mTotal / 200);
    QByteArray buf;

    while (!file.atEnd())
    {
        buf = file.read(bufSize);
        process->write(buf);
    }
}


/************************************************

 ************************************************/
void Encoder::runWav()
{
    QFile srcFile(mRequest.fileName());
    bool res =  srcFile.rename(mOutFile);

    if (!res)
    {
        error(mRequest.track(),
              tr("I can't rename file:\n%1 to %2\n%3").arg(
                  mRequest.fileName(),
                  mOutFile,
                  srcFile.errorString()));
    }

    emit trackProgress(mRequest.track(), Track::Encoding, 100);
    emit trackReady(mRequest.track(), mOutFile);
}
