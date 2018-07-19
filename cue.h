/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Flacon - audio File Encoder
 * https://github.com/flacon/flacon
 *
 * Copyright: 2012-2015
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


#ifndef CUE_H
#define CUE_H

#include <QString>
#include <QList>
#include "types.h"
#include "tagset.h"
#include "track.h"

class QFile;



class CueTagSet: public TagSet
{
public:
    explicit CueTagSet(const QString &uri);
    CueTagSet(const CueTagSet &other);

    QString cueFileName() const;
    QString fileTag() const;
    CueIndex index(int track, int indexNum) const;
    bool isMultiFileCue() const;
    int diskNumInCue() const;
};

class CueDisk
{
    friend class CueReader;
public:
    CueDisk();
    QString fileName() const { return mFileName; }
    const Tracks &tracks()   const { return mTracks; }
    Tracks &tracks()  { return mTracks; }
    DiskNum diskCount() const { return mDiskCount; }
    DiskNum diskNum() const { return mDiskNum; }
    QString uri() const { return mUri; }

private:
    QString mFileName;
    QString mUri;
    Tracks mTracks;
    DiskNum mDiskCount;
    DiskNum mDiskNum;
};

typedef QVector<CueDisk> Cue;

class CueReader
{
public:
    CueReader();
    QVector<CueDisk> load(const QString &fileName) noexcept(false);
};


class CueReaderError: public FlaconError
{
public:
    CueReaderError(const QString &msg): FlaconError(msg) {}
};

#endif // CUE_H
