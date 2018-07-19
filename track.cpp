/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Flacon - audio File Encoder
 * https://github.com/flacon/flacon
 *
 * Copyright: 2017
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


#include "track.h"

#include "disk.h"
#include "inputaudiofile.h"
#include "project.h"
#include "settings.h"
#include "outformat.h"

#include <QDir>
#include <QDebug>

/************************************************

 ************************************************/
Track::Track():
    TrackTags(),
    mStatus(NotRunning),
    mProgress(0),
    mTrackNum(0),
    mTrackCount(0),
    mDuration(0)
{
    qRegisterMetaType<Track::Status>("Track::Status");
}


/************************************************
 *
 ************************************************/
Track::Track(const TrackTags &tags):
    TrackTags(tags),
    mStatus(NotRunning),
    mProgress(0),
    mTrackNum(0),
    mTrackCount(0),
    mDuration(0)
{
    qRegisterMetaType<Track::Status>("Track::Status");
}


/************************************************
 *
 ************************************************/
Track::Track(const Track &other):
    TrackTags(other),
    mCueIndexes(other.mCueIndexes),
    mStatus(other.mStatus),
    mProgress(other.mProgress),
    mTrackNum(other.mTrackNum),
    mTrackCount(other.mTrackCount),
    mDuration(other.mDuration),
    mCueFileName(other.mCueFileName)
{
    qRegisterMetaType<Track::Status>("Track::Status");
}


/************************************************
 *
 ************************************************/
Track &Track::operator =(const Track &other)
{
    TrackTags::operator =(other);
    mCueIndexes = other.mCueIndexes;
    mStatus     = other.mStatus;
    mProgress   = other.mProgress;
    //mTags       = other.mTags;
    mTrackNum   = other.mTrackNum;
    mTrackCount = other.mTrackCount;
    mDuration   = other.mDuration;
    mCueFileName= other.mCueFileName;
    return *this;
}



/************************************************

 ************************************************/
Track::~Track()
{
}


/************************************************

 ************************************************/
void Track::setProgress(Track::Status status, int percent)
{
    mStatus = status;
    mProgress = percent;
    project->emitTrackProgress(this);
}


/************************************************

 ************************************************/
QString Track::resultFileName() const
{
    QString pattern = settings->outFilePattern();
    if (pattern.isEmpty())
        pattern = QString("%a/%y - %A/%n - %t");

    return calcFileName(pattern,
                        this->trackCount(),
                        this->trackNum(),
                        this->album(),
                        this->title(),
                        this->artist(),
                        this->genre(),
                        this->date(),
                        settings->outFormat()->ext());
}


/************************************************
  %N  Number of tracks       %n  Track number
  %a  Artist                 %A  Album title
  %y  Year                   %g  Genre
  %t  Track title
 ************************************************/
QString Track::calcFileName(const QString &pattern,
                            int trackCount,
                            int trackNum,
                            const QString &album,
                            const QString &title,
                            const QString &artist,
                            const QString &genre,
                            const QString &date,
                            const QString &fileExt)
{
    QHash<QChar, QString> tokens;
    tokens.insert(QChar('N'),   QString("%1").arg(trackCount, 2, 10, QChar('0')));
    tokens.insert(QChar('n'),   QString("%1").arg(trackNum, 2, 10, QChar('0')));
    tokens.insert(QChar('A'),   Disk::safeString(album));
    tokens.insert(QChar('t'),   Disk::safeString(title));
    tokens.insert(QChar('a'),   Disk::safeString(artist));
    tokens.insert(QChar('g'),   Disk::safeString(genre));
    tokens.insert(QChar('y'),   Disk::safeString(date));

    QString res = expandPattern(pattern, &tokens, false);
    return res + "." + fileExt;
}


/************************************************

 ************************************************/
QString Track::expandPattern(const QString &pattern, const QHash<QChar, QString> *tokens, bool optional)
{
    QString res;
    bool perc = false;
    bool hasVars = false;
    bool isValid = true;


    for(int i=0; i<pattern.length(); ++i)
    {
        QChar c = pattern.at(i);


        // Sub pattern .................................
        if (c == '{')
        {
            int level = 0;
            int start = i + 1;
            //int j = i;
            QString s = "{";

            for (int j=i; j<pattern.length(); ++j)
            {
                c = pattern.at(j);
                if (c == '{')
                    level++;
                else if (c == '}')
                    level--;

                if (level == 0)
                {
                    s = expandPattern(pattern.mid(start, j - start), tokens, true);
                    i = j;
                    break;
                }
            }
            res += s;
        }
        // Sub pattern .................................

        else
        {
            if (perc)
            {
                perc = false;
                if (tokens->contains(c))
                {
                    QString s = tokens->value(c);
                    hasVars = true;
                    isValid = !s.isEmpty();
                    res += s;
                }
                else
                {
                    if (c == '%')
                        res += "%";
                    else
                        res += QString("%") + c;
                }
            }
            else
            {
                if (c == '%')
                    perc = true;
                else
                    res += c;
            }
        }
    }

    if (perc)
        res += "%";

    if (optional)
    {
        if  (hasVars)
        {
            if (!isValid)
                return "";
        }
        else
        {
            return "{" + res + "}";
        }
    }

    return res;
}



/************************************************

 ************************************************/
QString Track::resultFilePath() const
{
    QString fileName = resultFileName();
    if (fileName.isEmpty())
        return "";

    QString dir = calcResultFilePath();
    if (dir.endsWith("/") || fileName.startsWith("/"))
        return calcResultFilePath() + fileName;
    else
        return calcResultFilePath() + "/" + fileName;
}


/************************************************

 ************************************************/
QString Track::calcResultFilePath() const
{
    QString dir = settings->outFileDir();

    if (dir == "~" || dir == "~//")
        return QDir::homePath();

    if (dir == ".")
        dir = "";

    if (dir.startsWith("~/"))
        return dir.replace(0, 1, QDir::homePath());

    QFileInfo fi(dir);

    if (fi.isAbsolute())
        return fi.absoluteFilePath();

    return QFileInfo(mCueFileName).dir().absolutePath() + QDir::separator() + dir;
}


/************************************************

 ************************************************/
CueIndex Track::cueIndex(int indexNum) const
{
    if (indexNum < mCueIndexes.length())
        return mCueIndexes.at(indexNum);

    return CueIndex();
}


/************************************************

 ************************************************/
void Track::setCueIndex(int indexNum, const CueIndex &value)
{
    if (indexNum >= mCueIndexes.length())
        mCueIndexes.resize(indexNum+1);

    mCueIndexes[indexNum] = value;
}

